#include <algorithm>
#include <cmath>
#include <limits>

#include "control/motion_controller.hpp"
#include "doctest/doctest.h"
#include "sim/drive.hpp"

using namespace frostbyte;

namespace {

// Testwerte, unabhängig von config/ und sim_params.json.
sim::DriveParams plant_params(double kA = 0.04) {
    sim::DriveParams p;
    p.kS_V = 0.6;
    p.kV_V_per_in_s = 0.2;
    p.kA_V_per_in_s2 = kA;
    p.spurbreite_in = 12.0;
    p.mu = 1.0;
    p.motor_max_V = 12.0;
    p.akku_V = 12.6;
    p.g_in_s2 = 386.1;
    return p;
}

MotionConfig test_config() {
    MotionConfig c;
    c.max_volts = 12.0;
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

PidGains lateral_gains() {
    PidGains g;
    g.kp = 1.0;
    g.ki = 0.0;
    g.kd = 0.08;
    g.out_max_v = 12.0;
    return g;
}

PidGains angular_gains() {
    PidGains g;
    g.kp = 0.15;
    g.ki = 0.5;
    g.kd = 0.01;
    g.i_zone = 10.0;
    g.i_max_v = 2.0;
    g.out_max_v = 12.0;
    return g;
}

Pose to_pose(const sim::Pose& p) { return Pose{p.x_in, p.y_in, p.h_deg}; }

// Regler alle 10 ms, Physik in 1-ms-Schritten, Pose aus dem Fahrmodell (Wahrheit).
struct Rig {
    explicit Rig(const sim::Pose& start, double kA = 0.04)
        : lat(lateral_gains()), ang(angular_gains()), mc(test_config(), lat, ang), model(plant_params(kA), start) {}

    // Läuft, bis der Befehl fertig ist (done/timeout/blocked) oder max_s vergangen sind.
    void run_until_finished(double max_s) {
        while (t_ms <= static_cast<int>(max_s * 1000.0)) {
            if (t_ms % 10 == 0) {
                out = mc.update(t_ms / 1000.0, to_pose(model.pose()));
                peak_v = std::max({peak_v, std::fabs(out.left_v), std::fabs(out.right_v)});
                if (mc.state() != MotionState::running)
                    return;
                track();
            }
            model.step(0.001, out.left_v, out.right_v);
            ++t_ms;
        }
    }

    double now_s() const { return t_ms / 1000.0; }
    virtual void track() {}
    virtual ~Rig() = default;

    Pid lat;
    Pid ang;
    MotionController mc;
    sim::DriveModel model;
    MotionOutput out;
    int t_ms = 0;
    double peak_v = 0.0;
};

double dist(const sim::Pose& p, double x, double y) { return std::hypot(p.x_in - x, p.y_in - y); }

}  // namespace

TEST_CASE("Motion: turn_to dreht auf 90 Grad und meldet DONE") {
    Rig r(sim::Pose{0.0, 0.0, 0.0});
    REQUIRE(r.mc.request(MotionCommand::turn_to(90.0, 3.0), 0.0));
    CHECK(r.mc.state() == MotionState::running);
    r.run_until_finished(5.0);
    CHECK(r.mc.state() == MotionState::done);
    CHECK(r.now_s() < 3.0);
    CHECK(std::fabs(wrap_deg(r.model.pose().h_deg - 90.0)) <= 2.0);
    CHECK(dist(r.model.pose(), 0.0, 0.0) < 1.0);  // Punktdrehung
    CHECK(r.peak_v <= 12.0 + 1e-9);
}

TEST_CASE("Motion: turn_to über ±180 nimmt den kurzen Weg (170 -> -170 = 20 Grad)") {
    struct Watch : Rig {
        using Rig::Rig;
        double min_abs_h = 180.0;
        void track() override { min_abs_h = std::min(min_abs_h, std::fabs(model.pose().h_deg)); }
    } r(sim::Pose{0.0, 0.0, 170.0});
    REQUIRE(r.mc.request(MotionCommand::turn_to(-170.0, 3.0), 0.0));
    r.run_until_finished(5.0);
    CHECK(r.mc.state() == MotionState::done);
    CHECK(std::fabs(wrap_deg(r.model.pose().h_deg + 170.0)) <= 2.0);
    CHECK(r.min_abs_h > 160.0);  // nie über 0 Grad gedreht
}

TEST_CASE("Motion: move_to fährt zu einem Punkt und meldet DONE") {
    Rig r(sim::Pose{0.0, 0.0, 0.0});
    REQUIRE(r.mc.request(MotionCommand::move_to(36.0, 24.0, 4.0), 0.0));
    r.run_until_finished(6.0);
    CHECK(r.mc.state() == MotionState::done);
    CHECK(dist(r.model.pose(), 36.0, 24.0) <= 1.0);
    CHECK(r.peak_v <= 12.0 + 1e-9);
}

TEST_CASE("Motion: move_to rückwärts fährt mit dem Heck voran") {
    Rig r(sim::Pose{0.0, 0.0, 0.0});
    REQUIRE(r.mc.request(MotionCommand::move_to(-30.0, 0.0, 4.0, true), 0.0));
    r.run_until_finished(6.0);
    CHECK(r.mc.state() == MotionState::done);
    CHECK(dist(r.model.pose(), -30.0, 0.0) <= 1.0);
    CHECK(std::fabs(r.model.pose().h_deg) < 10.0);  // nicht umgedreht
}

TEST_CASE("Motion: follow fährt einen L-Pfad ab, bleibt nah am Pfad und meldet DONE") {
    struct Watch : Rig {
        using Rig::Rig;
        double max_dev = 0.0;
        void track() override {
            // Abstand zum L aus (0,0)-(48,0) und (48,0)-(48,48)
            const sim::Pose p = model.pose();
            const double d1 = std::hypot(std::clamp(p.x_in, 0.0, 48.0) - p.x_in, p.y_in);
            const double d2 = std::hypot(48.0 - p.x_in, std::clamp(p.y_in, 0.0, 48.0) - p.y_in);
            max_dev = std::max(max_dev, std::min(d1, d2));
        }
    } r(sim::Pose{0.0, 0.0, 0.0});
    Path path;
    REQUIRE(path.add(0.0, 0.0));
    REQUIRE(path.add(48.0, 0.0));
    REQUIRE(path.add(48.0, 48.0));
    REQUIRE(r.mc.request(MotionCommand::follow(path, 6.0), 0.0));
    r.run_until_finished(8.0);
    CHECK(r.mc.state() == MotionState::done);
    CHECK(dist(r.model.pose(), 48.0, 48.0) <= 1.0);
    CHECK(r.max_dev < 6.0);  // Ecke wird mit Vorausschau abgekürzt, aber nicht weit
}

TEST_CASE("Motion: Ziel nicht in der Zeit erreichbar -> TIMEOUT, danach 0 V") {
    Rig r(sim::Pose{0.0, 0.0, 0.0});
    REQUIRE(r.mc.request(MotionCommand::move_to(200.0, 0.0, 1.0), 0.0));
    r.run_until_finished(3.0);
    CHECK(r.mc.state() == MotionState::timeout);
    CHECK(r.now_s() == doctest::Approx(1.0).epsilon(0.02));
    const MotionOutput o = r.mc.update(r.now_s() + 0.01, to_pose(r.model.pose()));
    CHECK(o.left_v == 0.0);
    CHECK(o.right_v == 0.0);
    CHECK(r.mc.state() == MotionState::timeout);  // bleibt bis zum nächsten Befehl
}

TEST_CASE("Motion: Roboter steckt fest (Pose ändert sich nicht trotz Spannung) -> BLOCKED vor dem Timeout") {
    for (const MotionCommand& cmd : {MotionCommand::move_to(24.0, 0.0, 5.0), MotionCommand::turn_to(90.0, 5.0)}) {
        Pid lat(lateral_gains()), ang(angular_gains());
        MotionController mc(test_config(), lat, ang);
        REQUIRE(mc.request(cmd, 0.0));
        double t = 0.0;
        MotionOutput o;
        for (; t < 5.0 && mc.state() == MotionState::running; t += 0.01)
            o = mc.update(t, Pose{0.0, 0.0, 0.0});
        CHECK(mc.state() == MotionState::blocked);
        CHECK(t <= 0.5 + 0.1);  // ein Fenster plus ein Takt
        o = mc.update(t + 0.01, Pose{0.0, 0.0, 0.0});
        CHECK(o.left_v == 0.0);
        CHECK(o.right_v == 0.0);
    }
}

TEST_CASE("Motion: träges Anfahren (großes kA) ist kein falsches BLOCKED; Überfahren -> zurücksetzen statt wenden") {
    struct Watch : Rig {
        using Rig::Rig;
        double max_abs_h = 0.0;
        double max_x = 0.0;
        void track() override {
            max_abs_h = std::max(max_abs_h, std::fabs(model.pose().h_deg));
            max_x = std::max(max_x, model.pose().x_in);
        }
    } r(sim::Pose{0.0, 0.0, 0.0}, 0.2);  // Zeitkonstante kA/kV = 1 s, die Testverstärkungen sind dafür zu scharf
    REQUIRE(r.mc.request(MotionCommand::move_to(48.0, 0.0, 8.0), 0.0));
    r.run_until_finished(10.0);
    CHECK(r.mc.state() == MotionState::done);
    CHECK(r.max_x > 48.0 + 4.0);  // fährt deutlich über das Ziel hinaus (über near_in) ...
    CHECK(r.max_abs_h < 5.0);     // ... und setzt zurück, ohne sich umzudrehen
}

TEST_CASE("Motion: Befehle ohne gültiges Timeout oder mit ungültigem Pfad werden abgelehnt") {
    Pid lat(lateral_gains()), ang(angular_gains());
    MotionController mc(test_config(), lat, ang);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (double timeout : {0.0, -1.0, inf, nan}) {
        CHECK_FALSE(mc.request(MotionCommand::turn_to(90.0, timeout), 0.0));
        CHECK(mc.state() == MotionState::idle);
    }
    CHECK_FALSE(mc.request(MotionCommand::move_to(nan, 0.0, 1.0), 0.0));
    CHECK_FALSE(mc.request(MotionCommand::turn_to(inf, 1.0), 0.0));

    Path empty;
    CHECK_FALSE(mc.request(MotionCommand::follow(empty, 1.0), 0.0));

    Path full;
    for (int i = 0; i < Path::capacity; ++i)
        REQUIRE(full.add(i, 0.0));
    CHECK_FALSE(full.add(99.0, 0.0));  // ein Punkt zu viel
    CHECK(full.overflowed());
    CHECK(full.size() == Path::capacity);
    CHECK_FALSE(mc.request(MotionCommand::follow(full, 1.0), 0.0));
    CHECK(mc.state() == MotionState::idle);

    Path bad;
    CHECK_FALSE(bad.add(nan, 0.0));
    CHECK_FALSE(mc.request(MotionCommand::follow(bad, 1.0), 0.0));

    const MotionOutput o = mc.update(0.01, Pose{});
    CHECK(o.left_v == 0.0);
    CHECK(o.right_v == 0.0);
}

TEST_CASE("Motion: ungültiger Befehl stoppt eine laufende Bewegung, neuer Befehl ersetzt den alten") {
    Pid lat(lateral_gains()), ang(angular_gains());
    MotionController mc(test_config(), lat, ang);
    REQUIRE(mc.request(MotionCommand::move_to(24.0, 0.0, 5.0), 0.0));
    CHECK(mc.update(0.0, Pose{}).left_v > 0.0);
    REQUIRE(mc.request(MotionCommand::turn_to(90.0, 5.0), 0.01));
    const MotionOutput turn = mc.update(0.01, Pose{});
    CHECK(turn.left_v < 0.0);  // gegen den Uhrzeigersinn: links rückwärts
    CHECK(turn.right_v > 0.0);
    CHECK_FALSE(mc.request(MotionCommand::turn_to(90.0, 0.0), 0.02));
    CHECK(mc.state() == MotionState::idle);
    const MotionOutput o = mc.update(0.03, Pose{});
    CHECK(o.left_v == 0.0);
    CHECK(o.right_v == 0.0);
}

TEST_CASE("Motion: ungültige Pose (NaN) gibt 0 V statt Unsinn") {
    Pid lat(lateral_gains()), ang(angular_gains());
    MotionController mc(test_config(), lat, ang);
    REQUIRE(mc.request(MotionCommand::move_to(24.0, 0.0, 5.0), 0.0));
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const MotionOutput o = mc.update(0.01, Pose{nan, 0.0, 0.0});
    CHECK(o.left_v == 0.0);
    CHECK(o.right_v == 0.0);
}

TEST_CASE("Motion: Pose-Ausfall länger als ein Blocked-Fenster -> danach weiter running, nicht BLOCKED oder DONE") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    {  // Roboter stand im Ausfall (0 V); erster gültiger Takt darf nicht mit dem alten Fenster verglichen werden.
        Pid lat(lateral_gains()), ang(angular_gains());
        MotionController mc(test_config(), lat, ang);
        REQUIRE(mc.request(MotionCommand::move_to(24.0, 0.0, 5.0), 0.0));
        CHECK(mc.update(0.0, Pose{}).left_v > 2.0);  // Fenster öffnet mit hoher Spannung
        for (double t = 0.01; t < 0.8; t += 0.01)
            mc.update(t, Pose{nan, 0.0, 0.0});
        const MotionOutput o = mc.update(0.8, Pose{});
        CHECK(mc.state() == MotionState::running);
        CHECK(o.left_v > 2.0);
        // Steckt er danach wirklich fest, kommt BLOCKED ein volles Fenster später.
        double t = 0.81;
        for (; t < 2.0 && mc.state() == MotionState::running; t += 0.01)
            mc.update(t, Pose{});
        CHECK(mc.state() == MotionState::blocked);
        CHECK(t >= 0.8 + 0.5);
    }
    {  // Im Ziel vor dem Ausfall: die Beruhigungszeit beginnt nach dem Ausfall neu.
        Pid lat(lateral_gains()), ang(angular_gains());
        MotionController mc(test_config(), lat, ang);
        REQUIRE(mc.request(MotionCommand::move_to(0.5, 0.0, 5.0), 0.0));
        mc.update(0.0, Pose{});  // im Ziel, Beruhigung beginnt
        for (double t = 0.01; t < 0.5; t += 0.01)
            mc.update(t, Pose{nan, 0.0, 0.0});
        mc.update(0.5, Pose{});
        CHECK(mc.state() == MotionState::running);  // nicht sofort DONE
        mc.update(0.61, Pose{});
        CHECK(mc.state() == MotionState::done);  // erst nach settle_s mit gültiger Pose
    }
}
