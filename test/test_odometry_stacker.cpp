#include <algorithm>
#include <cmath>
#include <limits>

#include "core/units.hpp"
#include "doctest/doctest.h"
#include "estimation/stacker_odometry.hpp"
#include "sim/drive.hpp"
#include "sim/sensor_models.hpp"

using namespace frostbyte;

namespace {

// Testwerte, bewusst verschieden von sim/sim_params.json und von config/.
sim::DriveParams drive_params() {
    sim::DriveParams p;
    p.kS_V = 1.0;
    p.kV_V_per_in_s = 0.2;
    p.kA_V_per_in_s2 = 0.04;
    p.spurbreite_in = 12.0;
    p.mu = 1.0;
    p.motor_max_V = 12.0;
    p.akku_V = 12.6;
    p.g_in_s2 = 386.1;
    return p;
}

constexpr double imu_offset_deg = 37.0;
constexpr double dt_s = 0.001;

double side_voltage(const sim::DriveParams& p, double v_set, double v_ist) {
    const double sign = v_set > 0.0 ? 1.0 : (v_set < 0.0 ? -1.0 : 0.0);
    return p.kS_V * sign + p.kV_V_per_in_s * v_set + 1.0 * (v_set - v_ist);
}

enum class Wheel { parallel, perpendicular, imu };
enum class Fault { none, jump, frozen, invalid, non_finite };

StackerOdometryParams odo_params() {
    StackerOdometryParams p;
    p.geometry.parallel_y_in = 2.5;        // Rad links von der Drehmitte
    p.geometry.perpendicular_x_in = -3.0;  // Rad hinter der Drehmitte
    p.drive.track_width_in = 12.0;
    p.drive.jump_deg = 10.0;
    p.drive.still_imu_deg = 0.05;
    p.drive.still_encoder_deg = 0.2;
    p.drive.still_cycles = 20;
    p.jump_in = 1.0;
    p.still_wheel_in = 0.01;
    p.still_drive_in = 0.1;
    p.still_cycles = 10;
    return p;
}

// Stacker-Aufbau: Antriebsencoder links/rechts, Parallel- und Querrad, IMU. Alle Messwerte entstehen aus den wahren
// Wegen des Fahrmodells. Ein Fehler an einem Sensor wird ab fail_cycle eingespeist.
struct Rig {
    Rig(const sim::Pose& start, const sim::OdometryParams& wheel_p, const sim::ImuParams& imu_p, double noise_scale,
        const StackerOdometryParams& op, int seed = 0)
        : p(drive_params()),
          drive(p, start),
          left(wheel_p, noise_scale, 1 + seed),
          right(wheel_p, noise_scale, 2 + seed),
          par(wheel_p, noise_scale, 3 + seed),
          perp(wheel_p, noise_scale, 4 + seed),
          imu(imu_p, noise_scale, 5 + seed),
          geo(op.geometry),
          odo(op) {
        l0 = drive.links_in();
        r0 = drive.rechts_in();
        h0 = drive.pose().h_deg;
        odo.reset(Pose{start.x_in, start.y_in, start.h_deg}, left.measured_in(), right.measured_in(), par.measured_in(),
                  perp.measured_in(), imu_true());
        prev_est = odo.pose();
        prev_truth = drive.pose();
    }

    double imu_true() { return wrap_deg(imu.measure_deg(drive.pose().h_deg, t_s) + imu_offset_deg); }

    bool faulty(Wheel w) const { return fault_wheel == w && fault != Fault::none && cycle >= fail_cycle; }

    // Messwert eines Rades mit eingespeistem Fehler.
    double wheel_reading(Wheel w, double v, double& frozen) const {
        if (!faulty(w)) {
            frozen = v;
            return v;
        }
        switch (fault) {
            case Fault::jump: return v + jump_in;
            case Fault::frozen: return frozen;
            case Fault::non_finite: return std::numeric_limits<double>::quiet_NaN();
            default: return v;
        }
    }

    double imu_reading() {
        const double v = imu_true();
        if (!faulty(Wheel::imu)) {
            imu_frozen = v;
            return v;
        }
        switch (fault) {
            case Fault::jump: return wrap_deg(v + 30.0);
            case Fault::frozen: return imu_frozen;
            case Fault::non_finite: return std::numeric_limits<double>::quiet_NaN();
            default: return v;
        }
    }

    // Ein Steuertakt (10 ms) Physik mit gleichbleibenden Seitentempi, danach Odometrie.
    void tick(double vl_set, double vr_set) {
        for (int k = 0; k < 10; ++k) {
            drive.step(dt_s, side_voltage(p, vl_set, drive.links_in_s()), side_voltage(p, vr_set, drive.rechts_in_s()));
            t_s += dt_s;
        }
        const double dl = drive.links_in() - l0, dr = drive.rechts_in() - r0;
        const double dtheta = deg_to_rad(wrap_deg(drive.pose().h_deg - h0));
        // Wahrer Radweg: Parallelrad rollt in x, Querrad in y (bei Drehung um den Versatz mitgedreht).
        // Antriebsschlupf: die Antriebsraeder drehen mehr als der Roboter faehrt (nur die Antriebsencoder sehen das).
        const double slip = cycle < slip_cycles ? slip_factor : 0.0;
        const double left_before = left.measured_in(), right_before = right.measured_in();
        left.step(dl * (1.0 + slip));
        right.step(dr * (1.0 + slip));
        par.step((dl + dr) / 2.0 - dtheta * geo.parallel_y_in);
        perp.step(dtheta * geo.perpendicular_x_in);
        l0 = drive.links_in();
        r0 = drive.rechts_in();
        h0 = drive.pose().h_deg;
        ++cycle;

        const double par_before = par.measured_in(), perp_before = perp.measured_in();
        const double par_read = wheel_reading(Wheel::parallel, par.measured_in(), par_frozen);
        const double perp_read = wheel_reading(Wheel::perpendicular, perp.measured_in(), perp_frozen);
        const bool inv = fault == Fault::invalid;
        odo.update(left.measured_in(), right.measured_in(), par_read, !(inv && faulty(Wheel::parallel)), perp_read,
                   !(inv && faulty(Wheel::perpendicular)), imu_reading(), !(inv && faulty(Wheel::imu)));

        // Abweichung der Raeder vom Erwartungswert aus Antriebsencodern und wahrer Drehung (ohne Fehler eingespeist).
        const double fwd = ((left.measured_in() - left_before) + (right.measured_in() - right_before)) / 2.0;
        max_res_par_in =
            std::max(max_res_par_in, std::fabs((par.measured_in() - par_before) - (fwd - dtheta * geo.parallel_y_in)));
        max_res_perp_in =
            std::max(max_res_perp_in, std::fabs((perp.measured_in() - perp_before) - dtheta * geo.perpendicular_x_in));

        const Pose est = odo.pose();
        const sim::Pose truth = drive.pose();
        const double err_x = est.x_in - truth.x_in, err_y = est.y_in - truth.y_in;
        const double err_h = wrap_deg(est.heading_deg - truth.h_deg);
        const double prev_err_x = prev_est.x_in - prev_truth.x_in, prev_err_y = prev_est.y_in - prev_truth.y_in;
        const double prev_err_h = wrap_deg(prev_est.heading_deg - prev_truth.h_deg);
        const double step_pos = std::hypot(err_x - prev_err_x, err_y - prev_err_y);
        const double step_head = std::fabs(wrap_deg(err_h - prev_err_h));
        if (odo.degraded() && !was_degraded) {
            switch_step_pos_in = step_pos;
            switch_step_head_deg = step_head;
        }
        was_degraded = odo.degraded();
        max_step_pos_in = std::max(max_step_pos_in, step_pos);
        max_step_head_deg = std::max(max_step_head_deg, step_head);
        max_pos_err_in = std::max(max_pos_err_in, std::hypot(err_x, err_y));
        max_head_err_deg = std::max(max_head_err_deg, std::fabs(err_h));
        prev_est = est;
        prev_truth = truth;
    }

    void run(int cycles, double vl_set, double vr_set) {
        for (int i = 0; i < cycles; ++i)
            tick(vl_set, vr_set);
    }

    sim::DriveParams p;
    sim::DriveModel drive;
    sim::OdometryModel left, right, par, perp;
    sim::ImuModel imu;
    TrackingGeometry geo;
    StackerOdometry odo;
    Wheel fault_wheel = Wheel::parallel;
    Fault fault = Fault::none;
    int fail_cycle = 0;
    double jump_in = 5.0;
    double par_frozen = 0.0, perp_frozen = 0.0, imu_frozen = 0.0;
    int cycle = 0;
    double l0 = 0.0, r0 = 0.0, h0 = 0.0, t_s = 0.0;
    Pose prev_est;
    sim::Pose prev_truth;
    double max_step_pos_in = 0.0, max_step_head_deg = 0.0;
    double switch_step_pos_in = 0.0, switch_step_head_deg = 0.0;  // Fehlersprung im Takt der Umschaltung
    bool was_degraded = false;
    double slip_factor = 0.0;
    int slip_cycles = 0;
    double max_res_par_in = 0.0, max_res_perp_in = 0.0;
    double max_pos_err_in = 0.0, max_head_err_deg = 0.0;
};

// Fahrprogramm mit Geradeaus, Bogen und Drehung, damit beide Räder gebraucht werden.
void drive_program(Rig& rig) {
    rig.run(100, 30.0, 30.0);
    rig.run(100, 20.0, 30.0);
    rig.run(80, -10.0, 10.0);
    rig.run(100, 30.0, 30.0);
}

}  // namespace

TEST_CASE("Stacker-Odometrie: Fahrt und Drehung ohne Fehler, kein Ausfall, Pose wie Tracking") {
    Rig rig(sim::Pose{-10.0, 5.0, 100.0}, sim::OdometryParams{}, sim::ImuParams{}, 0.0, odo_params());
    drive_program(rig);
    CHECK_FALSE(rig.odo.degraded());
    CHECK(rig.odo.fallback_reason() == StackerFallback::none);
    CHECK(rig.max_pos_err_in < 0.05);
    CHECK(rig.max_head_err_deg < 0.1);
}

TEST_CASE(
    "Stacker-Odometrie: Rauschen, Drift, Skalierungsfehler und Antriebsschlupf loesen keinen falschen Ausfall aus") {
    sim::OdometryParams wheels;
    wheels.scale_error = 0.05;  // 5 % Skalierungsfehler an allen Raedern
    wheels.sigma_in_per_tick = 0.0005;
    sim::ImuParams imu;
    imu.drift_deg_per_min = 2.0;
    imu.sigma_deg = 0.05;
    for (int seed = 0; seed < 5; ++seed) {
        Rig rig(sim::Pose{0.0, 0.0, -150.0 + 10.0 * seed}, wheels, imu, 1.0, odo_params(), 10 * seed);
        rig.run(300, 40.0, 40.0);    // Vollgas aus dem Stand (Antriebsraeder schlupfen im Modell)
        rig.run(200, 20.0, 40.0);    // Kreis
        rig.run(200, -12.0, 12.0);   // Punktdrehung
        rig.run(100, 0.0, 0.0);      // Stand
        rig.run(200, -40.0, -40.0);  // rueckwaerts
        CHECK_FALSE(rig.odo.degraded());
        MESSAGE("Seed ", seed, ": max. Abweichung Parallelrad ", rig.max_res_par_in, " in, Querrad ",
                rig.max_res_perp_in, " in (Grenze jump_in ", odo_params().jump_in, ")");
        CHECK(rig.max_res_par_in < odo_params().jump_in / 2.0);
        CHECK(rig.max_res_perp_in < odo_params().jump_in / 2.0);
    }
}

TEST_CASE("Stacker-Odometrie: Parallelrad springt, Umschaltung im selben Takt ohne Posensprung") {
    Rig rig(sim::Pose{0.0, 0.0, 20.0}, sim::OdometryParams{}, sim::ImuParams{}, 0.0, odo_params());
    rig.fault_wheel = Wheel::parallel;
    rig.fault = Fault::jump;
    rig.fail_cycle = 150;
    rig.run(149, 25.0, 25.0);
    CHECK_FALSE(rig.odo.degraded());
    rig.run(1, 25.0, 25.0);  // dieser Takt liefert den Sprung (5 in)
    CHECK(rig.odo.degraded());
    CHECK(rig.odo.fallback_reason() == StackerFallback::parallel_jump);
    CHECK(rig.switch_step_pos_in < 0.05);
    CHECK(rig.switch_step_head_deg < 0.1);
    rig.run(150, 20.0, 30.0);
    CHECK(rig.max_step_pos_in < 0.05);
    CHECK(rig.max_step_head_deg < 0.1);
    CHECK(rig.max_pos_err_in < 0.1);
}

TEST_CASE("Stacker-Odometrie: Querrad springt waehrend einer Drehung") {
    Rig rig(sim::Pose{0.0, 0.0, 0.0}, sim::OdometryParams{}, sim::ImuParams{}, 0.0, odo_params());
    rig.fault_wheel = Wheel::perpendicular;
    rig.fault = Fault::jump;
    rig.fail_cycle = 40;
    rig.run(40, -10.0, 10.0);
    CHECK(rig.odo.fallback_reason() == StackerFallback::perpendicular_jump);
    rig.run(100, 25.0, 25.0);
    CHECK(rig.max_step_pos_in < 0.05);
    CHECK(rig.max_pos_err_in < 0.1);
}

TEST_CASE(
    "Stacker-Odometrie: Parallelrad friert ein, erkannt nach still_cycles, Pose laeuft schon im Verdacht richtig") {
    Rig rig(sim::Pose{0.0, 0.0, 90.0}, sim::OdometryParams{}, sim::ImuParams{}, 0.0, odo_params());
    rig.fault_wheel = Wheel::parallel;
    rig.fault = Fault::frozen;
    rig.fail_cycle = 80;
    rig.run(80, 30.0, 30.0);
    CHECK_FALSE(rig.odo.degraded());
    rig.run(5, 30.0, 30.0);  // Verdacht, noch nicht bestaetigt
    CHECK_FALSE(rig.odo.degraded());
    rig.run(10, 30.0, 30.0);
    CHECK(rig.odo.degraded());
    CHECK(rig.odo.fallback_reason() == StackerFallback::parallel_stall);
    CHECK(rig.switch_step_pos_in < 0.05);
    rig.run(100, 20.0, 30.0);
    // Auch die Verdachtstakte rechnen mit den Antriebsencodern: kein Zurueckbleiben um still_cycles * Weg je Takt.
    CHECK(rig.max_step_pos_in < 0.05);
    CHECK(rig.max_pos_err_in < 0.1);
}

TEST_CASE("Stacker-Odometrie: Querrad friert bei Drehung ein") {
    Rig rig(sim::Pose{0.0, 0.0, 0.0}, sim::OdometryParams{}, sim::ImuParams{}, 0.0, odo_params());
    rig.fault_wheel = Wheel::perpendicular;
    rig.fault = Fault::frozen;
    rig.fail_cycle = 20;
    rig.run(20, 25.0, 25.0);
    CHECK_FALSE(rig.odo.degraded());  // Geradeaus liest das Querrad ohnehin ~0: nicht sichtbar, nicht schlimm
    rig.run(100, -30.0, 30.0);  // schnelle Drehung: erwarteter Querradweg 0,15 in je Takt, sonst unter der Schwelle
    CHECK(rig.odo.fallback_reason() == StackerFallback::perpendicular_stall);
    CHECK(rig.switch_step_pos_in < 0.05);
    MESSAGE("Querrad eingefroren: max. Positionsfehler ", rig.max_pos_err_in, " in");
    CHECK(rig.max_pos_err_in < 1.0);  // gemessen 0,64 in: der unentdeckte Anlauf der Drehung
    // Grenze: im Anlauf der Drehung liegt der Erwartungswert unter still_drive_in; dort bleibt ein eingefrorenes Rad
    // unentdeckt, je Takt geht höchstens diese Schwelle als Fehler ein.
    CHECK(rig.max_step_pos_in <= odo_params().still_drive_in);
}

TEST_CASE("Stacker-Odometrie: ungueltiges Rad (HAL) und NaN schalten sofort um, Pose bleibt endlich") {
    for (Wheel w : {Wheel::parallel, Wheel::perpendicular}) {
        for (Fault f : {Fault::invalid, Fault::non_finite}) {
            Rig rig(sim::Pose{0.0, 0.0, 0.0}, sim::OdometryParams{}, sim::ImuParams{}, 0.0, odo_params());
            rig.fault_wheel = w;
            rig.fault = f;
            rig.fail_cycle = 30;
            rig.run(29, 20.0, 30.0);
            CHECK_FALSE(rig.odo.degraded());
            rig.run(1, 20.0, 30.0);
            CHECK(rig.odo.degraded());
            CHECK(rig.switch_step_pos_in < 0.05);
            rig.run(100, 20.0, 30.0);
            CHECK(std::isfinite(rig.odo.pose().x_in));
            CHECK(std::isfinite(rig.odo.pose().heading_deg));
            CHECK(rig.max_step_pos_in < 0.05);
            CHECK(rig.max_pos_err_in < 0.1);
        }
    }
}

TEST_CASE("Stacker-Odometrie: IMU-Ausfall (ungueltig, Sprung, eingefroren) schaltet auf Antriebsencoder") {
    for (Fault f : {Fault::invalid, Fault::non_finite, Fault::jump}) {
        Rig rig(sim::Pose{0.0, 0.0, 0.0}, sim::OdometryParams{}, sim::ImuParams{}, 0.0, odo_params());
        rig.fault_wheel = Wheel::imu;
        rig.fault = f;
        rig.fail_cycle = 30;
        rig.run(130, 20.0, 30.0);
        CHECK(rig.odo.fallback_reason() == StackerFallback::imu);
        CHECK(std::isfinite(rig.odo.pose().heading_deg));
        CHECK(rig.max_step_head_deg < 0.1);
        CHECK(rig.max_pos_err_in < 0.1);
    }
    Rig rig(sim::Pose{0.0, 0.0, -90.0}, sim::OdometryParams{}, sim::ImuParams{}, 0.0, odo_params());
    rig.fault_wheel = Wheel::imu;
    rig.fault = Fault::frozen;
    rig.fail_cycle = 50;
    rig.run(50, 25.0, 25.0);
    rig.run(100, -12.0, 12.0);
    CHECK(rig.odo.fallback_reason() == StackerFallback::imu);
    CHECK(rig.max_head_err_deg < 1.0);
    CHECK(rig.max_pos_err_in < 0.5);
}

TEST_CASE("Stacker-Odometrie: Rueckfall ist endgueltig bis reset") {
    StackerOdometry odo(odo_params());
    odo.reset(Pose{}, 0.0, 0.0, 0.0, 0.0, 0.0);
    odo.update(0.0, 0.0, 0.0, false, 0.0, true, 0.0);
    REQUIRE(odo.degraded());
    CHECK(odo.fallback_reason() == StackerFallback::parallel_invalid);
    for (int i = 1; i <= 10; ++i)
        odo.update(0.1 * i, 0.1 * i, 0.1 * i, true, 0.0, true, 0.0);
    CHECK(odo.degraded());
    CHECK(odo.fallback_reason() == StackerFallback::parallel_invalid);
    odo.reset(Pose{}, 0.0, 0.0, 0.0, 0.0, 0.0);
    CHECK_FALSE(odo.degraded());
}

TEST_CASE("Stacker-Odometrie: Verdacht, der sich loest, schaltet nicht um") {
    StackerOdometry odo(odo_params());
    odo.reset(Pose{}, 0.0, 0.0, 0.0, 0.0, 0.0);
    double l = 0.0, par = 0.0;
    for (int i = 0; i < 9; ++i) {  // 9 Takte Stillstand des Rads bei Fahrt (< still_cycles = 10)
        l += 0.3;
        odo.update(l, l, par, true, 0.0, true, 0.0);
    }
    for (int i = 0; i < 20; ++i) {  // Rad liest wieder
        l += 0.3;
        par += 0.3;
        odo.update(l, l, par, true, 0.0, true, 0.0);
    }
    CHECK_FALSE(odo.degraded());
    for (int i = 0; i < 9; ++i) {  // Zaehler wurde zurueckgesetzt: wieder 9 Takte ohne Umschaltung
        l += 0.3;
        odo.update(l, l, par, true, 0.0, true, 0.0);
    }
    CHECK_FALSE(odo.degraded());
}
