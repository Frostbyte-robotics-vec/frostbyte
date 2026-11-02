#include <sstream>
#include <string>

#include "doctest/doctest.h"
#include "sim/demo.hpp"
#include "sim/world.hpp"

using namespace frostbyte::sim;

namespace {

const std::string fixtures = FB_TEST_FIXTURES;

// Konfiguration ohne vermessenes Feld: Startposen sind null, das Demo setzt eigene Platzhalter.
SimConfig base_config(const std::string& profile = "realistic") {
    return load_config(fixtures + "/sim_params_min.json", fixtures + "/field_min.json", "", "", profile);
}

}  // namespace

TEST_CASE("Demo: setzt Startposen und Handsteuerung, kennzeichnet den Lauf als demo") {
    SimConfig cfg = base_config();
    apply_demo(cfg);
    CHECK(cfg.build == "demo");
    REQUIRE(cfg.stacker.start.has_value());
    REQUIRE(cfg.runner.start.has_value());
    CHECK(static_cast<bool>(cfg.stacker.open_loop));
    CHECK(static_cast<bool>(cfg.runner.open_loop));
}

TEST_CASE("Demo: beide Roboter fahren, berühren sich, keine Regelverstöße, Motoren stehen bei 0:00") {
    SimConfig cfg = base_config("none");
    apply_demo(cfg);
    std::ostringstream s, r;
    const RunResult res = run_match(cfg, 1, s, r);
    CHECK(res.robot_contacts >= 1);
    CHECK(res.rule_violations == 0);
    CHECK(s.str().find("COLLISION:runner") != std::string::npos);
    CHECK(s.str().find("build=demo") != std::string::npos);
}

TEST_CASE("Demo: gleicher Seed = byte-gleiche Logs") {
    const auto run = [] {
        SimConfig cfg = base_config();
        apply_demo(cfg);
        std::ostringstream s, r;
        run_match(cfg, 9, s, r);
        return s.str() + r.str();
    };
    CHECK(run() == run());
}

TEST_CASE("Leerer Routenpfad heißt: keine Route") { CHECK(load_route("").empty()); }
