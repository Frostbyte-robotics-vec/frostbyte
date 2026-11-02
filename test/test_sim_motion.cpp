#include <cmath>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "control/motion_controller.hpp"
#include "doctest/doctest.h"
#include "sim/motion_adapter.hpp"
#include "sim/world.hpp"

// Sim-Adapter (D-05): der MotionController regelt das Fahrmodell in run_match mit der geschätzten Pose.
namespace {

using frostbyte::MotionCommand;
using frostbyte::MotionConfig;
using frostbyte::MotionController;
using frostbyte::MotionState;
using frostbyte::Pid;
using frostbyte::PidGains;
using namespace frostbyte::sim;

const std::string fixtures = FB_TEST_FIXTURES;
constexpr int col_tx = 9, col_ty = 10;

// Startpose im Testfeld: (-60, 12,5), Blick +y (90 Grad).
SimConfig one_robot(const std::string& profile) {
    return load_config(fixtures + "/sim_params_min.json", fixtures + "/field_min.json", fixtures + "/empty.route",
                       fixtures + "/empty.route", profile);
}

// Testwerte, unabhängig von config/.
MotionConfig test_config() {
    MotionConfig c;
    c.max_volts = 11.5;
    c.track_width_in = 12.0;
    c.turn_tol_deg = 2.0;
    c.move_tol_in = 1.0;
    c.settle_s = 0.1;
    c.near_in = 4.0;
    c.lookahead_in = 10.0;
    c.follow_max_volts = 8.0;
    c.blocked_window_s = 0.5;
    c.blocked_min_effort_v = 2.0;
    c.blocked_min_motion_in = 0.5;
    c.blocked_min_motion_deg = 2.0;
    return c;
}

PidGains lateral() {
    PidGains g;
    g.kp = 1.0;
    g.kd = 0.08;
    g.out_max_v = 12.0;
    return g;
}

PidGains angular() {
    PidGains g;
    g.kp = 0.15;
    g.ki = 0.5;
    g.kd = 0.01;
    g.i_zone = 10.0;
    g.i_max_v = 2.0;
    g.out_max_v = 12.0;
    return g;
}

std::vector<std::string> cells(const std::string& line) {
    std::vector<std::string> out;
    std::stringstream ss(line);
    std::string c;
    while (std::getline(ss, c, ','))
        out.push_back(c);
    return out;
}

std::string last_line(const std::string& csv) {
    const std::size_t end = csv.find_last_not_of('\n');
    return csv.substr(csv.rfind('\n', end) + 1, end - csv.rfind('\n', end));
}

}  // namespace

TEST_CASE("Sim-Adapter: move_to im Match mit Schätzpose kommt an und meldet DONE") {
    SimConfig cfg = one_robot("none");
    Pid lat(lateral()), ang(angular());
    MotionController mc(test_config(), lat, ang);
    REQUIRE(mc.request(MotionCommand::move_to(-36.0, 36.0, 5.0), 0.0));
    int calls = 0;
    const auto inner = motion_loop(mc);
    cfg.stacker.closed_loop = [&](int t_ms, const Pose& est) {
        ++calls;
        return inner(t_ms, est);
    };
    cfg.stacker.open_loop = [](int) { return std::make_pair(12.0, -12.0); };  // closed_loop hat Vorrang

    std::ostringstream s, r;
    const RunResult res = run_match(cfg, 1, s, r);
    CHECK(mc.state() == MotionState::done);
    CHECK(calls == res.control_ticks);  // genau einmal je Steuertakt
    const auto c = cells(last_line(s.str()));
    CHECK(std::hypot(std::stod(c[col_tx]) + 36.0, std::stod(c[col_ty]) - 36.0) <= 1.0);
    CHECK(res.rule_violations == 0);
}

TEST_CASE("Sim-Adapter: Regler, der bei 0:00 noch Spannung will, ist ein Regelverstoß; Motoren stehen trotzdem") {
    SimConfig cfg = one_robot("none");
    cfg.stacker.closed_loop = [](int t_ms, const Pose&) {
        return t_ms >= 59990 ? std::make_pair(3.0, 3.0) : std::make_pair(0.0, 0.0);
    };
    std::ostringstream s, r;
    const RunResult res = run_match(cfg, 1, s, r);
    CHECK(res.rule_violations == 1);
    CHECK(s.str().find("RULE:MOTORS_END") != std::string::npos);
}

TEST_CASE("Sim-Adapter: gleicher Seed = byte-gleiches Log auch mit Regelung (Profil realistic)") {
    auto once = [](std::uint64_t seed) {
        SimConfig cfg = one_robot("realistic");
        Pid lat(lateral()), ang(angular());
        MotionController mc(test_config(), lat, ang);
        frostbyte::Path path;
        path.add(-60.0, 12.5);
        path.add(-60.0, 40.0);
        path.add(-30.0, 40.0);
        mc.request(MotionCommand::follow(path, 6.0), 0.0);
        cfg.stacker.closed_loop = motion_loop(mc);
        std::ostringstream s, r;
        run_match(cfg, seed, s, r);
        return s.str();
    };
    const std::string a = once(7);
    CHECK(a == once(7));
    CHECK(a != once(8));
}
