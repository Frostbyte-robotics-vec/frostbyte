#include "sim/world.hpp"

#include <cmath>
#include <fstream>
#include <ostream>
#include <stdexcept>

#include "core/log_format.hpp"
#include "sim/json.hpp"
#include "sim/rules.hpp"

namespace frostbyte::sim {

std::uint64_t Rng::next() {
    std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

double Rng::uniform() { return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0); }

namespace {

constexpr double pi = 3.14159265358979323846;

int positive_ms(const Json& welt, const char* key) {
    const double v = welt.at(key).as_number();
    const int ms = static_cast<int>(v);
    if (ms <= 0 || ms != v)
        throw std::runtime_error(std::string("sim_params welt.") + key + " muss eine positive ganze Zahl sein");
    return ms;
}

std::optional<Pose> start_pose(const Json& field, const std::string& id) {
    const Json& posen = field.at("posen");
    for (std::size_t i = 0; i < posen.size(); ++i) {
        const Json& p = posen[i];
        if (p.at("id").as_string() != id)
            continue;
        // null = noch nicht vermessen: keine Pose erfinden.
        if (p.at("x_in").is_null() || p.at("y_in").is_null() || p.at("h_deg").is_null())
            return std::nullopt;
        return Pose{p.at("x_in").as_number(), p.at("y_in").as_number(), p.at("h_deg").as_number()};
    }
    throw std::runtime_error("field.json: Pose " + id + " fehlt");
}

class RobotLog {
public:
    RobotLog(const RobotSetup& robot, std::uint64_t seed, const std::string& build, std::ostream& out)
        : robot_(robot), out_(out) {
        char buf[256];
        if (logfmt::format_header(buf, sizeof buf, robot.name.c_str(), "sim", static_cast<std::int64_t>(seed),
                                  build.c_str()) >= static_cast<int>(sizeof buf))
            throw std::runtime_error("Log-Kopfzeile länger als " + std::to_string(sizeof buf) +
                                     " Byte (build zu lang?)");
        out_ << buf << '\n' << logfmt::columns << '\n';
    }

    // truth: wahre Pose aus dem Fahrmodell; leer, solange die Startpose null ist (keine Pose erfinden).
    // est: Pose aus Odometrie und IMU; gps: letzte GPS-Messung (nur wenn gültig); event: Ereignisse seit der letzten Zeile.
    // score: Punktestand des Teams aus dem Scorer; leer, solange die Simulation keinen Spielzustand führt.
    void row(int t_ms, const std::optional<Pose>& truth, const std::optional<Pose>& est, const GpsSample* gps,
             const std::string& event, const std::optional<int>& score) {
        logfmt::Row r;
        r.t_s = t_ms / 1000.0;
        r.robot = robot_.name.c_str();
        if (truth) {
            r.tx_in = truth->x_in;
            r.ty_in = truth->y_in;
            r.th_deg = truth->h_deg;
        }
        if (est) {
            r.x_in = est->x_in;
            r.y_in = est->y_in;
            r.h_deg = est->h_deg;
        }
        if (gps && gps->valid) {
            r.gx_in = gps->x_in;
            r.gy_in = gps->y_in;
            r.gh_deg = gps->heading_deg;
            r.g_err_in = gps->error_in;
        }
        if (score)
            r.score = *score;
        if (!event.empty())
            r.event = event.c_str();
        char buf[512];
        if (logfmt::format_row(buf, sizeof buf, r) >= static_cast<int>(sizeof buf))
            throw std::runtime_error("Log-Zeile länger als " + std::to_string(sizeof buf) + " Byte (" + robot_.name +
                                     ", t = " + std::to_string(t_ms) + " ms)");
        out_ << buf << '\n';
    }

private:
    const RobotSetup& robot_;
    std::ostream& out_;
};

// Alles, was ein Roboter im Lauf braucht: Fahrmodell (wahre Größen), Sensormodelle, Schätzpose, Ereignisse.
struct RobotRun {
    RobotRun(const RobotSetup& s, std::uint64_t seed) : setup(s), sensor_seed(seed) {}

    const RobotSetup& setup;
    std::uint64_t sensor_seed;
    std::optional<DriveModel> drive;
    std::optional<OdometryModel> odo;
    std::optional<ImuModel> imu;
    std::optional<GpsModel> gps;
    std::optional<Pose> est;
    GpsSample gps_sample;
    double last_dist_in = 0.0;
    double last_measured_in = 0.0;
    std::pair<double, double> held_cmd{0.0, 0.0};  // letzte Ausgabe von closed_loop
    std::string events;                            // seit dem letzten Log-Takt, mit ';' getrennt
    bool wall_contact = false;
    bool robot_contact = false;
    int wall_free_steps = 0;
    int robot_free_steps = 0;

    void add_event(const std::string& e) {
        if (!events.empty())
            events += ';';
        events += e;
    }
    double dist_in() const { return (drive->links_in() + drive->rechts_in()) / 2.0; }
    double speed_in_s() const { return (drive->links_in_s() + drive->rechts_in_s()) / 2.0; }
    Box box() const {
        const Pose p = drive->pose();
        return Box{p.x_in, p.y_in, setup.length_in, setup.width_in, p.h_deg};
    }
};

// Ein Kontakt beginnt mit Ereignis und Zähler einmal und endet erst nach so vielen Schritten ohne Berührung
// (schiebende Roboter berühren sich sonst bei jeder Auflösung neu).
constexpr int contact_release_steps = 50;

// Gibt true zurück, wenn der Kontakt in diesem Schritt neu begonnen hat.
bool track_contact(bool touching, bool& flag, int& free_steps) {
    if (touching) {
        free_steps = 0;
        if (!flag) {
            flag = true;
            return true;
        }
    } else if (flag && ++free_steps >= contact_release_steps) {
        flag = false;
        free_steps = 0;
    }
    return false;
}

}  // namespace

SimConfig load_config(const std::string& params_path, const std::string& field_path, const std::string& stacker_route,
                      const std::string& runner_route, const std::string& noise_profile) {
    const Json params = load_json_file(params_path);
    const Json field = load_json_file(field_path);
    const Json& welt = params.at("welt");

    SimConfig cfg;
    cfg.noise_profile = noise_profile;
    cfg.schritt_ms = positive_ms(welt, "schritt_ms");
    cfg.steuer_ms = positive_ms(welt, "steuer_ms");
    cfg.log_ms = positive_ms(welt, "log_ms");
    if (cfg.steuer_ms % cfg.schritt_ms != 0 || cfg.log_ms % cfg.schritt_ms != 0)
        throw std::runtime_error("sim_params welt: steuer_ms und log_ms müssen Vielfache von schritt_ms sein");
    if (match_ms % cfg.log_ms != 0)
        throw std::runtime_error("sim_params welt: log_ms muss die Matchdauer (" + std::to_string(match_ms) +
                                 " ms) ohne Rest teilen");
    cfg.field_in = field.at("feld_in").as_number();

    const auto robot = [&](const char* name, const char* start_id, const std::string& route) {
        RobotSetup r;
        r.name = name;
        r.start = start_pose(field, start_id);
        r.route = load_route(route);
        r.drive = load_drive_params(params, name);
        r.sensors = load_sensor_params(params, name, cfg.noise_profile);
        const Json& umriss = params.at("roboter").at(name).at("umriss_in");
        r.length_in = umriss.at("laenge").as_number();
        r.width_in = umriss.at("breite").as_number();
        return r;
    };
    cfg.stacker = robot("stacker", "START_STACKER", stacker_route);
    cfg.runner = robot("runner", "START_RUNNER", runner_route);
    return cfg;
}

std::vector<std::string> load_route(const std::string& path) {
    if (path.empty())
        return {};  // keine Route (z. B. frostbyte_sim --demo)
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("Route nicht lesbar: " + path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        const auto first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == '#')
            continue;
        lines.push_back(line.substr(first));
    }
    return lines;
}

RunResult run_match(const SimConfig& cfg, std::uint64_t seed, std::ostream& stacker_csv, std::ostream& runner_csv) {
    Rng rng(seed);  // leitet je Roboter einen eigenen Seed für die Sensormodelle ab
    RobotLog stacker_log(cfg.stacker, seed, cfg.build, stacker_csv);
    RobotLog runner_log(cfg.runner, seed, cfg.build, runner_csv);
    RobotRun runs[2] = {RobotRun(cfg.stacker, rng.next()), RobotRun(cfg.runner, rng.next())};
    RobotLog* logs[2] = {&stacker_log, &runner_log};
    // Eigene Zufallsströme des Spiels, nach den Sensor-Seeds gezogen: die Posen bleiben mit und ohne Spielzustand gleich.
    std::optional<Game> game;
    if (cfg.game.enabled()) {
        const std::uint64_t seed_stacker = rng.next();
        const std::uint64_t seed_runner = rng.next();
        game.emplace(cfg.game, seed_stacker, seed_runner, match_ms);
    }

    RunResult result;
    // Ohne Startpose kein Fahrmodell: es gibt nichts, wovon die wahre Pose ausgehen koennte.
    for (RobotRun& r : runs) {
        const RobotSetup& s = r.setup;
        if (!s.start)
            continue;
        r.drive.emplace(s.drive, *s.start);
        r.odo.emplace(s.sensors.odometry, s.sensors.noise_scale, r.sensor_seed);
        r.imu.emplace(s.sensors.imu, s.sensors.noise_scale, r.sensor_seed);
        r.gps.emplace(s.sensors.gps, s.sensors.noise_scale, r.sensor_seed);
        r.est = *s.start;  // der Roboter kennt seine Startpose
        // Höhe ist nicht modelliert (steht nicht in sim_params.json): SG3 wird hier nicht geprüft.
        RobotRuleState rs;
        rs.length_in = s.length_in;
        rs.width_in = s.width_in;
        for (const RuleViolation& v : check_rules(rs)) {
            r.add_event("RULE:" + v.rule);
            ++result.rule_violations;
        }
    }
    const double dt_s = cfg.schritt_ms / 1000.0;
    const auto truth_of = [](const RobotRun& r) {
        return r.drive ? std::optional<Pose>(r.drive->pose()) : std::nullopt;
    };

    // Zeit als ganzzahlige ms; t_s wird pro Zeile neu berechnet, nie aufaddiert.
    // Je ms: Steuerung (auf ihrem Raster), Befehle, Log des Zustands bei t_ms, dann Physik t_ms -> t_ms + schritt_ms.
    for (int t_ms = 0; t_ms <= match_ms; t_ms += cfg.schritt_ms) {
        // Steuerung zuerst (sie ändert weder Spannungen der Handsteuerung noch Ereignisse), damit closed_loop im selben ms wirkt.
        if (t_ms % cfg.steuer_ms == 0) {
            ++result.control_ticks;
            for (RobotRun& r : runs) {
                if (!r.drive)
                    continue;
                const double d = r.dist_in();
                r.odo->step(d - r.last_dist_in);
                r.last_dist_in = d;
                const double step_in = r.odo->measured_in() - r.last_measured_in;
                r.last_measured_in = r.odo->measured_in();
                const double h_deg = r.imu->measure_deg(r.drive->pose().h_deg, t_ms / 1000.0);
                const double h_rad = h_deg * pi / 180.0;
                r.est = Pose{r.est->x_in + step_in * std::cos(h_rad), r.est->y_in + step_in * std::sin(h_rad), h_deg};
                if (r.setup.closed_loop)
                    r.held_cmd = r.setup.closed_loop(t_ms, *r.est);
            }
        }
        double volts[2][2] = {{0.0, 0.0}, {0.0, 0.0}};  // ohne Regler und Handsteuerung 0 V (Haftreibung hält)
        for (int i = 0; i < 2; ++i) {
            RobotRun& r = runs[i];
            if (!r.drive || (!r.setup.closed_loop && !r.setup.open_loop))
                continue;
            const auto cmd = r.setup.closed_loop ? r.held_cmd : r.setup.open_loop(t_ms);
            volts[i][0] = cmd.first;
            volts[i][1] = cmd.second;
            if (t_ms >= match_ms) {  // Matchende 0:00: alle Motoren Halt, sonst Regelverstoß
                if (!motors_stopped({cmd.first, cmd.second})) {
                    r.add_event("RULE:MOTORS_END");
                    ++result.rule_violations;
                }
                volts[i][0] = volts[i][1] = 0.0;
            }
        }

        if (game)
            game->step(t_ms);
        if (t_ms % cfg.log_ms == 0) {
            const std::optional<int> score = game ? std::optional<int>(game->score()) : std::nullopt;
            for (int i = 0; i < 2; ++i) {
                RobotRun& r = runs[i];
                if (r.drive) {
                    const Pose p = r.drive->pose();
                    r.gps_sample = r.gps->measure(p.x_in, p.y_in, p.h_deg, r.speed_in_s());
                }
                logs[i]->row(t_ms, truth_of(r), r.est, r.drive ? &r.gps_sample : nullptr, r.events, score);
                r.events.clear();
            }
            ++result.log_rows;
        }

        for (int i = 0; i < 2; ++i)
            if (runs[i].drive)
                runs[i].drive->step(dt_s, volts[i][0], volts[i][1]);

        // Kollisionen: Roboter untereinander (je zur Hälfte auseinander), dann Feldrand.
        if (runs[0].drive && runs[1].drive) {
            const auto mtv = penetration(runs[0].box(), runs[1].box());
            if (mtv) {
                const Pose a = runs[0].drive->pose();
                const Pose b = runs[1].drive->pose();
                runs[0].drive->set_position(a.x_in + mtv->x_in / 2.0, a.y_in + mtv->y_in / 2.0);
                runs[1].drive->set_position(b.x_in - mtv->x_in / 2.0, b.y_in - mtv->y_in / 2.0);
            }
            const bool began0 = track_contact(mtv.has_value(), runs[0].robot_contact, runs[0].robot_free_steps);
            const bool began1 = track_contact(mtv.has_value(), runs[1].robot_contact, runs[1].robot_free_steps);
            if (began0)
                runs[0].add_event("COLLISION:" + runs[1].setup.name);
            if (began1)
                runs[1].add_event("COLLISION:" + runs[0].setup.name);
            if (began0)
                ++result.robot_contacts;
        }
        for (RobotRun& r : runs) {
            if (!r.drive)
                continue;
            const Vec2 push = push_inside_field(r.box(), cfg.field_in);
            const bool wall = push.x_in != 0.0 || push.y_in != 0.0;
            if (wall) {
                const Pose p = r.drive->pose();
                r.drive->set_position(p.x_in + push.x_in, p.y_in + push.y_in);
            }
            if (track_contact(wall, r.wall_contact, r.wall_free_steps)) {
                r.add_event("COLLISION:wall");
                ++result.wall_contacts;
            }
        }
    }
    return result;
}

}  // namespace frostbyte::sim
