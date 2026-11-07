#include <cmath>
#include <cstring>
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

// Antriebs-Prüfungen (D-07): Funktionstest in der Simulation, kein Nachweis der Reglergüte. Testwerte statt config/ und sim_params.json.
namespace {
std::string root_path(const std::string& rel) { return std::string(FB_ROOT_DIR) + "/" + rel; }

struct Truth {
    double x, y, h;
};
Truth final_pose(const std::string& csv) {
    const std::size_t end = csv.find_last_not_of('\n');
    std::stringstream ss(csv.substr(csv.rfind('\n', end) + 1));
    std::vector<std::string> c;
    for (std::string cell; std::getline(ss, cell, ',');)
        c.push_back(cell);
    return {std::stod(c.at(9)), std::stod(c.at(10)), std::stod(c.at(11))};
}

// Aufsummierte Drehung der wahren Richtung (Spalte 12) über alle Zeilen; max_back = größter Rücklauf gegen die Drehrichtung.
struct Rotation {
    double net = 0.0, max_back = 0.0;
};
Rotation rotation(const std::string& csv) {
    std::stringstream ss(csv);
    std::string line;
    std::getline(ss, line);  // Kopfzeile
    std::getline(ss, line);  // Spaltennamen
    std::vector<double> hs;
    while (std::getline(ss, line)) {
        std::stringstream ls(line);
        std::vector<std::string> c;
        for (std::string cell; std::getline(ls, cell, ',');)
            c.push_back(cell);
        if (c.size() > 11 && !c[11].empty())
            hs.push_back(std::stod(c[11]));
    }
    double total = 0.0, lo = 0.0, hi = 0.0, drop = 0.0, rise = 0.0;
    for (std::size_t i = 1; i < hs.size(); ++i) {
        total += std::remainder(hs[i] - hs[i - 1], 360.0);
        lo = std::min(lo, total);
        hi = std::max(hi, total);
        drop = std::max(drop, hi - total);
        rise = std::max(rise, total - lo);
    }
    return {total, total > 0 ? drop : rise};
}

Run run_drive(const std::string& route) {
    SimConfig cfg = load_config(fixtures + "/sim_params_min.json", fixtures + "/field_drive.json", root_path(route),
                                fixtures + "/empty.route");
    cfg.stacker.control = test_control();
    cfg.runner.control = test_control();
    return run(cfg);
}
}  // namespace

TEST_CASE("Antriebs-Prüfung: drive_straight läuft in der Simulation durch und endet nahe dem Ziel (Funktionstest)") {
    const Run r = run_drive("routes/drive_straight.route");
    const Truth t = final_pose(r.stacker);
    CHECK(std::hypot(t.x - 39.37, t.y) <= 3.0);  // grob: die Simulation ist hier ungetunt
    CHECK(has(r.stacker, "goto_pose"));
}

TEST_CASE("Antriebs-Prüfung: drive_turns läuft 40 Drehungen und endet nahe Richtung 0 (Funktionstest)") {
    const Run r = run_drive("routes/drive_turns.route");
    const Truth t = final_pose(r.stacker);
    double dh = std::fmod(t.h, 360.0);
    if (dh > 180.0)
        dh -= 360.0;
    CHECK(std::fabs(dh) <= 10.0);                // grob
    CHECK(std::hypot(t.x + 39.37, t.y) <= 3.0);  // dreht am Ort
    // 10 Umdrehungen am Stück in einer Richtung (nicht hin und her, nicht übersprungen)
    const Rotation rot = rotation(r.stacker);
    CHECK(std::fabs(std::fabs(rot.net) - 3600.0) <= 20.0);
    CHECK(rot.max_back <= 5.0);
    // alle 40 Drehungen sind vor Matchende durch: der Routenschritt ist am Ende leer (end hold erreicht)
    const std::size_t last = r.stacker.rfind('\n', r.stacker.find_last_not_of('\n'));
    CHECK(r.stacker.substr(last + 1).find("turn") == std::string::npos);
}

TEST_CASE("Routen: goto <NAME> tol= erreicht den MotionController (D-14)") {
    using namespace frostbyte;
    // Pose fest 3 in vor dem Ziel (kürzer als das Blockiert-Fenster): nur mit weiter Toleranz meldet der Ausführer das goto als erledigt.
    auto status_after_settle = [](double tol_in) {
        MotionConfig mcfg = test_control().motion;
        mcfg.move_tol_in = 1.0;
        mcfg.settle_s = 0.1;
        Pid lat(test_control().lateral), ang(test_control().angular);
        MotionController mc(mcfg, lat, ang);
        std::vector<FieldPoint> points(1);
        points[0].id = "P";
        points[0].xy_in = std::make_pair(3.0, 0.0);
        SignalBus bus;
        SimRouteExecutor exec(mc, points, bus, RouteRobot::stacker, RoutePlayerConfig{});
        RouteCmd cmd;
        cmd.op = RouteOp::goto_name;
        std::strcpy(cmd.name, "P");
        cmd.tol_in = tol_in;
        exec.start(0, cmd, 0.0);
        for (int t_ms = 0; t_ms <= 300; t_ms += 10)
            mc.update(t_ms / 1000.0, frostbyte::Pose{0.0, 0.0, 0.0});
        return exec.poll(0, 0.3);
    };
    CHECK(status_after_settle(6.0) == StepStatus::done);
    CHECK(status_after_settle(-1.0) == StepStatus::running);  // ohne tol=: move_tol_in
    CHECK(status_after_settle(0.25) == StepStatus::running);
}
