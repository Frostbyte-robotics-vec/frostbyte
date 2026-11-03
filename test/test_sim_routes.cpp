#include <cmath>
#include <sstream>
#include <string>
#include <vector>

#include "doctest/doctest.h"
#include "sim/world.hpp"

// Routen in der Simulation (A-04, ADR-031): beide Roboter spielen ihre Route gleichzeitig durch den RoutePlayer.
// Testfeld test/fixtures/field_route.json und Testwerte statt config/ und field/field.json (Platzhalter).
namespace {

using namespace frostbyte::sim;

const std::string fixtures = FB_TEST_FIXTURES;

RouteControl test_control() {
    RouteControl c;
    c.motion.max_volts = 11.5;
    c.motion.track_width_in = 12.0;
    c.motion.follow_max_volts = 8.0;
    c.lateral.kp = 1.0;
    c.lateral.kd = 0.08;
    c.lateral.out_max_v = 12.0;
    c.angular.kp = 0.15;
    c.angular.ki = 0.5;
    c.angular.kd = 0.01;
    c.angular.i_zone = 10.0;
    c.angular.i_max_v = 2.0;
    c.angular.out_max_v = 12.0;
    return c;
}

SimConfig load(const std::string& stacker, const std::string& runner, const std::string& noise = "none") {
    SimConfig cfg = load_config(fixtures + "/sim_params_min.json", fixtures + "/field_route.json",
                                fixtures + "/" + stacker, fixtures + "/" + runner, noise);
    cfg.stacker.control = test_control();
    cfg.runner.control = test_control();
    return cfg;
}

struct Run {
    RunResult res;
    std::string stacker, runner;
};

Run run(const SimConfig& cfg, std::uint64_t seed = 1) {
    std::ostringstream s, r;
    Run out;
    out.res = run_match(cfg, seed, s, r);
    out.stacker = s.str();
    out.runner = r.str();
    return out;
}

bool has(const std::string& text, const std::string& what) { return text.find(what) != std::string::npos; }

// Wahre Pose (tx, ty: Spalten 9 und 10 laut docs/log_format.md) der letzten Zeile.
std::pair<double, double> final_truth(const std::string& csv) {
    const std::size_t end = csv.find_last_not_of('\n');
    std::stringstream ss(csv.substr(csv.rfind('\n', end) + 1));
    std::vector<std::string> c;
    for (std::string cell; std::getline(ss, cell, ',');)
        c.push_back(cell);
    return {std::stod(c.at(9)), std::stod(c.at(10))};
}

}  // namespace

TEST_CASE("Routen: beide Roboter fahren auf denselben Punkt, die Kollision wird erkannt") {
    const Run r = run(load("route_meet_stacker.route", "route_meet_runner.route"));
    CHECK(r.res.robot_contacts >= 1);
    CHECK(has(r.stacker, "COLLISION:runner"));
    CHECK(has(r.runner, "COLLISION:stacker"));
}

TEST_CASE("Routen: getrennte Ziele ergeben keine Kollision (Gegenprobe)") {
    const Run r = run(load("route_apart_stacker.route", "route_apart_runner.route"));
    CHECK(r.res.robot_contacts == 0);
    CHECK_FALSE(has(r.stacker, "COLLISION"));
    CHECK_FALSE(has(r.runner, "COLLISION"));
}

TEST_CASE(
    "Routen: der Roboter fährt wirklich (wahre Pose am Ende nahe dem Ziel) und die Spalte route_step ist gefüllt") {
    const Run r = run(load("route_apart_stacker.route", "route_apart_runner.route"));
    CHECK(has(r.stacker, "2:goto_pose"));
    const auto t = final_truth(r.stacker);
    CHECK(std::hypot(t.first + 20.0, t.second + 40.0) <= 1.5);
}

TEST_CASE("Routen: gleicher Seed = byte-gleiche Logs, anderer Seed nicht (Profil realistic)") {
    const SimConfig cfg = load("route_apart_stacker.route", "route_apart_runner.route", "realistic");
    const Run a = run(cfg, 7), b = run(cfg, 7), c = run(cfg, 8);
    CHECK(a.stacker == b.stacker);
    CHECK(a.runner == b.runner);
    CHECK(a.stacker != c.stacker);
}

TEST_CASE("Routen: toggle in der Stacker-Route wird beim Laden abgelehnt, am Runner ist es erlaubt") {
    CHECK_THROWS_WITH_AS(load("route_toggle.route", "empty.route"), doctest::Contains("toggle gibt es nur am Runner"),
                         std::runtime_error);
    CHECK_NOTHROW(load("empty.route", "route_toggle.route"));
}

TEST_CASE("Routen: goto auf einen Punkt ohne Koordinaten scheitert, die Route läuft weiter") {
    const Run r = run(load("route_nowhere.route", "empty.route"));
    // Ziel (-20, 20) der zweiten Anweisung erreicht: der Roboter ist nicht nach (0, 0) gefahren
    const auto t = final_truth(r.stacker);
    CHECK(std::hypot(t.first + 20.0, t.second - 20.0) <= 1.5);
    CHECK_FALSE(has(r.stacker, "COLLISION"));
}

TEST_CASE("Routen: wait_for wartet auf das signal des anderen Roboters") {
    // Ohne Sender läuft wait_for ins Timeout (3 s) und die Route fährt trotzdem weiter; mit Sender sofort.
    const Run with = run(load("route_wait.route", "route_signal.route"));
    const Run without = run(load("route_wait.route", "empty.route"));
    const auto first_goto = [](const std::string& csv) {
        const std::size_t p = csv.find("3:goto_pose");
        if (p == std::string::npos)
            return -1.0;
        return std::stod(csv.substr(csv.rfind('\n', p) + 1));
    };
    CHECK(first_goto(with.stacker) >= 0.0);
    CHECK(first_goto(with.stacker) < 1.0);
    CHECK(first_goto(without.stacker) >= 3.0);
}

TEST_CASE("Routen: die Skills-Routen aus routes/ lassen sich laden und mit field/field.json parsen") {
    const std::string root = FB_ROOT_DIR;
    const SimConfig cfg = load_config(root + "/sim/sim_params.json", root + "/field/field.json",
                                      root + "/routes/skills_stacker.route", root + "/routes/skills_runner.route");
    CHECK_FALSE(cfg.stacker.cmds.empty());
    CHECK_FALSE(cfg.runner.cmds.empty());
}
