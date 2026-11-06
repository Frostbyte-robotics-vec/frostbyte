#include "sim/world.hpp"

#include <cmath>
#include <fstream>
#include <memory>
#include <ostream>
#include <stdexcept>

#include "core/log_format.hpp"
#include "sim/json.hpp"
#include "sim/route_parser.hpp"
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

struct GameCols {
    int score;
    int has_cup;
    int has_pin;
    double lift_deg;
    double lift_set_deg;
};

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
    // game: Punktestand des Teams aus dem Scorer und Besitz dieses Roboters; leer, solange die Simulation keinen
    // Spielzustand führt.
    void row(int t_ms, const std::optional<Pose>& truth, const std::optional<Pose>& est, const GpsSample* gps,
             const std::string& event, const std::string& route_step, const std::optional<GameCols>& game) {
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
        if (game) {
            r.score = game->score;
            r.has_cup = game->has_cup;
            r.has_pin = game->has_pin;
            r.lift_deg = game->lift_deg;
            r.lift_set_deg = game->lift_set_deg;
        }
        if (!route_step.empty())
            r.route_step = route_step.c_str();
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
    std::unique_ptr<RouteRunner> route;            // nur mit Route (A-04) und Startpose
    std::string route_step;                        // Stand beim letzten Steuertakt
    bool wall_contact = false;
    bool robot_contact = false;
    bool height_over = false;  // SG3 gerade überschritten (SIM-32)
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

// Goals aus field.json als Anfangszustand des Spielzustands (leere Stapel). Zone aus dem Quadranten (RED_*, BLUE_*,
// MIDFIELD), nicht aus typ. Der Toggle-Anfangszustand steht nicht in field.json: Annahme spiel.toggle_anfang.
void add_route_goals(const Json& params, const Json& field, GameSetup& g) {
    load_game_params(params, g);
    const std::string toggle_start = params.at("spiel").at("toggle_anfang").as_string();
    if (toggle_start != "zielfarbe" && toggle_start != "neutral")
        throw std::runtime_error("sim_params spiel.toggle_anfang: 'zielfarbe' oder 'neutral' erwartet");
    const Json& goals = field.at("goals");
    std::vector<std::string> goal_quadrants;
    for (std::size_t i = 0; i < goals.size(); ++i) {
        const Json& j = goals[i];
        const std::string q = j.at("quadrant").as_string();
        GoalState s;
        if (q.rfind("RED_", 0) == 0)
            s.zone = Zone::red_quadrant;
        else if (q.rfind("BLUE_", 0) == 0)
            s.zone = Zone::blue_quadrant;
        else if (q == "MIDFIELD")
            s.zone = Zone::midfield;
        else
            throw std::runtime_error("field.json goals: Quadrant '" + q + "' unbekannt");
        if (toggle_start == "zielfarbe" && s.zone != Zone::midfield)
            s.toggle = s.zone == Zone::red_quadrant ? ToggleColor::red : ToggleColor::blue;
        g.goals.push_back(std::move(s));
        g.goal_names.push_back(j.at("id").as_string());
        goal_quadrants.push_back(q);
    }
    // Toggles (SIM-23): ein Toggle setzt die Goals seines Quadranten auf seine Zielfarbe.
    if (const Json* toggles = field.find("toggles")) {
        for (std::size_t i = 0; i < toggles->size(); ++i) {
            const Json& j = (*toggles)[i];
            const std::string color = j.at("zielfarbe").as_string();
            if (color != "red" && color != "blue")
                throw std::runtime_error("field.json toggles: zielfarbe '" + color + "' unbekannt (red oder blue)");
            ToggleSpec t;
            t.name = j.at("id").as_string();
            t.x_in = j.at("x_in").as_number();
            t.y_in = j.at("y_in").as_number();
            t.color = color == "red" ? ToggleColor::red : ToggleColor::blue;
            for (std::size_t k = 0; k < goal_quadrants.size(); ++k)
                if (goal_quadrants[k] == j.at("quadrant").as_string())
                    t.goals.push_back(static_cast<int>(k));
            if (t.goals.empty())
                throw std::runtime_error("field.json toggles: " + t.name + ": kein Goal im Quadranten");
            g.toggles.push_back(std::move(t));
        }
    }
    if (g.goals.empty())
        throw std::runtime_error("field.json: Routen mit cycle/intake brauchen Goals");
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
    cfg.game.midfield = load_midfield(field);

    std::optional<RouteNames> names;  // erst, wenn eine Route Zeilen hat (field_min.json u. ä. haben nicht alle Blöcke)
    cfg.points = load_field_points(field);
    cfg.objects = load_field_objects(params, field);
    const auto robot = [&](const char* name, RouteRobot self, const char* start_id, const std::string& route) {
        RobotSetup r;
        r.name = name;
        r.self = self;
        r.start = start_pose(field, start_id);
        r.route = load_route(route);
        if (!r.route.empty()) {
            std::string text;
            for (const std::string& line : r.route)
                text += line + '\n';
            if (!names)
                names = route_names_from_field(field_path);
            RouteResult parsed = parse_route(text, *names);
            if (!parsed.ok())
                throw std::runtime_error(route + ": " + parsed.error);
            // Der Parser kennt den Roboter nicht (ADR-023): toggle gibt es nur am Runner.
            for (const RouteCmd& c : parsed.cmds)
                if (c.op == RouteOp::toggle && self != RouteRobot::runner)
                    throw std::runtime_error(route + ": Zeile " + std::to_string(c.line) +
                                             ": toggle gibt es nur am Runner");
            r.cmds = std::move(parsed.cmds);
        }
        r.drive = load_drive_params(params, name);
        r.sensors = load_sensor_params(params, name, cfg.noise_profile);
        const Json& umriss = params.at("roboter").at(name).at("umriss_in");
        r.length_in = umriss.at("laenge").as_number();
        r.width_in = umriss.at("breite").as_number();
        return r;
    };
    cfg.stacker = robot("stacker", RouteRobot::stacker, "START_STACKER", stacker_route);
    cfg.runner = robot("runner", RouteRobot::runner, "START_RUNNER", runner_route);
    // Spielzustand (SIM-22, ADR-032) nur, wenn eine Route Aufnahme oder Zyklen hat und das Feld Goals kennt: sonst bleiben
    // intake/cycle Platzhalter und sim_params.json braucht weder ereignisse noch spiel.
    const auto uses_game = [](const RobotSetup& r) {
        for (const RouteCmd& c : r.cmds)
            if (c.op == RouteOp::cycle || c.op == RouteOp::intake)
                return true;
        return false;
    };
    if (uses_game(cfg.stacker) || uses_game(cfg.runner))
        add_route_goals(params, field, cfg.game);
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
    ObjectField objects = cfg.objects;  // Zustand der Objekte gehört zum Lauf, cfg bleibt unverändert
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
        // SG3 (Höhe) prüft die Schleife unten in jedem Schritt aus dem Lift-Winkel (SIM-32); beim Start steht der Lift unten.
        RobotRuleState rs;
        rs.length_in = s.length_in;
        rs.width_in = s.width_in;
        for (const RuleViolation& v : check_rules(rs)) {
            r.add_event("RULE:" + v.rule);
            ++result.rule_violations;
        }
    }
    // Routen (A-04): ein Spieler je Roboter mit Startpose; beide teilen den Signalbus. Handsteuerung und Regelung gehen vor.
    SignalBus bus;
    for (RobotRun& r : runs)
        if (r.drive && !r.setup.cmds.empty() && !r.setup.closed_loop)
            r.route = std::make_unique<RouteRunner>(r.setup.cmds, cfg.points, bus, r.setup.self, r.setup.control,
                                                    game ? &*game : nullptr,
                                                    FloorObjects{&objects, r.setup.length_in, r.setup.width_in});
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
                if (r.setup.closed_loop) {
                    r.held_cmd = r.setup.closed_loop(t_ms, *r.est);
                } else if (r.route) {
                    r.held_cmd = r.route->step(t_ms, *r.est, r.drive->pose());
                    r.route_step = r.route->step_text();
                }
            }
        }
        double volts[2][2] = {{0.0, 0.0}, {0.0, 0.0}};  // ohne Regler und Handsteuerung 0 V (Haftreibung hält)
        for (int i = 0; i < 2; ++i) {
            RobotRun& r = runs[i];
            if (!r.drive || (!r.setup.closed_loop && !r.route && !r.setup.open_loop))
                continue;
            const auto cmd = (r.setup.closed_loop || r.route) ? r.held_cmd : r.setup.open_loop(t_ms);
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

        if (game) {
            game->step(t_ms);
            for (int i = 0; i < 2; ++i)
                for (const std::string& e : game->take_events(i, t_ms))
                    runs[i].add_event(e);
            // SG3 in jedem Schritt (SIM-32): ein Ereignis je Überschreitung, nicht je Schritt. Ohne Höhentabelle nichts.
            for (int i = 0; i < 2; ++i) {
                const std::optional<double> h = game->lift_height_in(i);
                if (!h)
                    continue;
                RobotRun& r = runs[i];
                RobotRuleState rs;
                rs.height_in = *h;
                bool over = false;
                for (const RuleViolation& v : check_rules(rs))
                    over = over || v.rule == "SG3";
                if (over && !r.height_over) {
                    r.add_event("RULE:SG3");
                    ++result.rule_violations;
                }
                r.height_over = over;
            }
        }
        if (t_ms % cfg.log_ms == 0) {
            int mid = 0;
            if (game) {
                std::vector<Box> boxes;
                for (const RobotRun& r : runs)
                    if (r.drive)
                        boxes.push_back(r.box());
                mid = robots_in_midfield(cfg.game.midfield, boxes);
            }
            for (int i = 0; i < 2; ++i) {
                RobotRun& r = runs[i];
                if (r.drive) {
                    const Pose p = r.drive->pose();
                    r.gps_sample = r.gps->measure(p.x_in, p.y_in, p.h_deg, r.speed_in_s());
                }
                logs[i]->row(t_ms, truth_of(r), r.est, r.drive ? &r.gps_sample : nullptr, r.events, r.route_step,
                             game ? std::optional<GameCols>(GameCols{game->score(mid), game->cups(i), game->pins(i),
                                                                     game->lift_deg(i), game->lift_set_deg(i)})
                                  : std::nullopt);
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
        // Feldobjekte: erst umstoßen, dann schieben (SIM-30); Roboter weicht um seinen Anteil.
        for (RobotRun& r : runs) {
            if (!r.drive)
                continue;
            const ObjectContact oc = objects.step(r.box(), cfg.field_in);
            for (const std::string& id : oc.toppled) {
                r.add_event("TOPPLED:" + id);
                ++result.object_topples;
            }
            if (oc.robot_shift.x_in != 0.0 || oc.robot_shift.y_in != 0.0) {
                r.drive->displace(oc.robot_shift.x_in, oc.robot_shift.y_in);
            }
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
    for (const FieldObject& o : objects.objects())
        result.object_picks += o.taken ? 1 : 0;
    return result;
}

}  // namespace frostbyte::sim
