#include <cmath>
#include <sstream>
#include <string>
#include <vector>

#include "doctest/doctest.h"
#include "sim/world.hpp"

using namespace frostbyte::sim;

// SIM-44: GPS-Fusion im Steuertakt der Simulation. Die Fusionswerte stehen hier im Test (Fixture), nicht aus
// sim_params.json (dort Platzhalter).
namespace {

const std::string fixtures = FB_TEST_FIXTURES;

// Stacker fährt 2 s mit 6 V geradeaus (+y), danach steht er; der Wegmesser hat 10 % Skalierungsfehler.
SimConfig drifting_config() {
    SimConfig cfg = load_config(fixtures + "/sim_params_min.json", fixtures + "/field_min.json",
                                fixtures + "/empty.route", fixtures + "/empty.route");
    cfg.stacker.sensors.odometry.scale_error = 0.1;
    cfg.stacker.closed_loop = [](int t_ms, const Pose&) {
        return t_ms < 2000 ? std::make_pair(6.0, 6.0) : std::make_pair(0.0, 0.0);
    };
    cfg.stacker.sensors.fusion = {true, {3.0, 5.0, 20.0, 0.1, 0.0, 20}};
    return cfg;
}

std::vector<std::string> last_cells(const std::string& log) {
    const std::string last = log.substr(log.rfind('\n', log.size() - 2) + 1);
    std::vector<std::string> cells;
    std::istringstream row(last.substr(0, last.size() - 1));
    for (std::string c; std::getline(row, c, ',');)
        cells.push_back(c);
    return cells;
}

// Abstand Schätzpose (x_in, y_in) zur wahren Pose (tx_in, ty_in) in der letzten Logzeile.
double final_error_in(const std::string& log) {
    const auto c = last_cells(log);
    REQUIRE(c.size() >= 12);
    return std::hypot(std::stod(c[2]) - std::stod(c[9]), std::stod(c[3]) - std::stod(c[10]));
}

double run_error_in(SimConfig cfg, std::uint64_t seed) {
    std::ostringstream s, r;
    run_match(cfg, seed, s, r);
    return final_error_in(s.str());
}

}  // namespace

TEST_CASE("Fusion: ohne Fusion bleibt der Wegmesser-Fehler stehen (Gegenprobe)") {
    SimConfig cfg = drifting_config();
    cfg.stacker.sensors.fusion.enabled = false;
    CHECK(run_error_in(cfg, 1) > 2.0);
}

TEST_CASE("Fusion: mit GPS wird die Schätzpose auf höchstens 1 in an die wahre Pose gezogen") {
    for (std::uint64_t seed = 1; seed <= 5; ++seed)
        CHECK(run_error_in(drifting_config(), seed) <= 1.0);
}

TEST_CASE("Fusion: ohne GPS (nur Aussetzer) bleibt es bei der Odometrie") {
    SimConfig cfg = drifting_config();
    cfg.stacker.sensors.gps.dropout_fraction = 1.0;
    CHECK(run_error_in(cfg, 1) > 2.0);
}

TEST_CASE("Fusion: gleicher Seed = byte-gleiches Log") {
    std::ostringstream s1, r1, s2, r2;
    run_match(drifting_config(), 3, s1, r1);
    run_match(drifting_config(), 3, s2, r2);
    CHECK(s1.str() == s2.str());
    CHECK(r1.str() == r2.str());
}
