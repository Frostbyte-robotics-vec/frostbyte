#include "sim/world.hpp"

#include <fstream>
#include <ostream>
#include <stdexcept>

#include "core/log_format.hpp"
#include "sim/json.hpp"

namespace frostbyte::sim {

std::uint64_t Rng::next() {
    std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

double Rng::uniform() { return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0); }

namespace {

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
            throw std::runtime_error("Log-Kopfzeile länger als " + std::to_string(sizeof buf) + " Byte (build zu lang?)");
        out_ << buf << '\n' << logfmt::columns << '\n';
    }

    // truth: wahre Pose aus dem Fahrmodell; leer, solange die Startpose null ist (keine Pose erfinden).
    void row(int t_ms, const std::optional<Pose>& truth) {
        logfmt::Row r;
        r.t_s = t_ms / 1000.0;
        r.robot = robot_.name.c_str();
        if (truth) {
            r.tx_in = truth->x_in;
            r.ty_in = truth->y_in;
            r.th_deg = truth->h_deg;
        }
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

}  // namespace

SimConfig load_config(const std::string& params_path, const std::string& field_path, const std::string& stacker_route,
                      const std::string& runner_route) {
    const Json params = load_json_file(params_path);
    const Json field = load_json_file(field_path);
    const Json& welt = params.at("welt");

    SimConfig cfg;
    cfg.schritt_ms = positive_ms(welt, "schritt_ms");
    cfg.steuer_ms = positive_ms(welt, "steuer_ms");
    cfg.log_ms = positive_ms(welt, "log_ms");
    if (cfg.steuer_ms % cfg.schritt_ms != 0 || cfg.log_ms % cfg.schritt_ms != 0)
        throw std::runtime_error("sim_params welt: steuer_ms und log_ms müssen Vielfache von schritt_ms sein");
    if (match_ms % cfg.log_ms != 0)
        throw std::runtime_error("sim_params welt: log_ms muss die Matchdauer (" + std::to_string(match_ms) +
                                 " ms) ohne Rest teilen");

    cfg.stacker = {"stacker", start_pose(field, "START_STACKER"), load_route(stacker_route),
                   load_drive_params(params, "stacker")};
    cfg.runner = {"runner", start_pose(field, "START_RUNNER"), load_route(runner_route),
                  load_drive_params(params, "runner")};
    return cfg;
}

std::vector<std::string> load_route(const std::string& path) {
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
    Rng rng(seed);  // Quelle für Rauschen, sobald Fahr- und Sensormodelle (SIM-02 ff.) es brauchen
    (void)rng;
    RobotLog stacker(cfg.stacker, seed, cfg.build, stacker_csv);
    RobotLog runner(cfg.runner, seed, cfg.build, runner_csv);
    // Ohne Startpose kein Fahrmodell: es gibt nichts, wovon die wahre Pose ausgehen koennte.
    std::optional<DriveModel> stacker_drive, runner_drive;
    if (cfg.stacker.start)
        stacker_drive.emplace(cfg.stacker.drive, *cfg.stacker.start);
    if (cfg.runner.start)
        runner_drive.emplace(cfg.runner.drive, *cfg.runner.start);
    const auto truth = [](const std::optional<DriveModel>& d) {
        return d ? std::optional<Pose>(d->pose()) : std::nullopt;
    };
    const double dt_s = cfg.schritt_ms / 1000.0;

    RunResult result;
    // Zeit als ganzzahlige ms; t_s wird pro Zeile neu berechnet, nie aufaddiert.
    // Je ms: Steuerung (auf ihrem Raster), Log des Zustands bei t_ms, dann Physik t_ms -> t_ms + schritt_ms.
    for (int t_ms = 0; t_ms <= match_ms; t_ms += cfg.schritt_ms) {
        if (t_ms % cfg.steuer_ms == 0)
            ++result.control_ticks;  // Steuerung beider Roboter: folgt mit dem Routen-Ticket
        if (t_ms % cfg.log_ms == 0) {
            stacker.row(t_ms, truth(stacker_drive));
            runner.row(t_ms, truth(runner_drive));
            ++result.log_rows;
        }
        // Noch kein Regler: 0 V, die Haftreibung haelt die Roboter an der Startpose.
        if (stacker_drive)
            stacker_drive->step(dt_s, 0.0, 0.0);
        if (runner_drive)
            runner_drive->step(dt_s, 0.0, 0.0);
    }
    return result;
}

}  // namespace frostbyte::sim
