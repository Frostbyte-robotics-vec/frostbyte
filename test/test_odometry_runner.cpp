#include <algorithm>
#include <cmath>
#include <limits>

#include "core/units.hpp"
#include "doctest/doctest.h"
#include "estimation/drive_odometry.hpp"
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

constexpr double imu_offset_deg = 37.0;  // Einbaulage des IMU gegenueber dem Feld
constexpr double dt_s = 0.001;

double side_voltage(const sim::DriveParams& p, double v_set, double v_ist) {
    const double sign = v_set > 0.0 ? 1.0 : (v_set < 0.0 ? -1.0 : 0.0);
    return p.kS_V * sign + p.kV_V_per_in_s * v_set + 1.0 * (v_set - v_ist);
}

enum class Fault { none, jump, frozen, invalid, non_finite };

// Runner-Aufbau wie am Roboter: Wegsummen beider Antriebsseiten plus IMU. Die Messwerte kommen aus den Seitenwegen des
// Fahrmodells (nicht aus der wahren Pose). Ein IMU-Fehler wird ab fail_cycle eingespeist.
struct Rig {
    Rig(const sim::Pose& start, const sim::OdometryParams& enc_p, const sim::ImuParams& imu_p, double noise_scale,
        const DriveOdometryParams& op)
        : p(drive_params()),
          drive(p, start),
          left(enc_p, noise_scale, 1),
          right(enc_p, noise_scale, 2),
          imu(imu_p, noise_scale, 3),
          odo(op) {
        l0 = drive.links_in();
        r0 = drive.rechts_in();
        odo.reset(Pose{start.x_in, start.y_in, start.h_deg}, left.measured_in(), right.measured_in(), imu_reading());
        prev_est = odo.pose();
        prev_truth = drive.pose();
    }

    double imu_true() { return wrap_deg(imu.measure_deg(drive.pose().h_deg, t_s) + imu_offset_deg); }

    double imu_reading() {
        const double v = imu_true();
        if (fault == Fault::none || cycle < fail_cycle)
            return frozen_value = v;
        switch (fault) {
            case Fault::jump: return wrap_deg(v + jump_deg);
            case Fault::frozen: return frozen_value;
            case Fault::non_finite: return std::numeric_limits<double>::quiet_NaN();
            default: return v;
        }
    }

    bool imu_valid() const { return !(fault == Fault::invalid && cycle >= fail_cycle); }

    // Ein Steuertakt (10 ms) Physik mit gleichbleibenden Seitentempi, danach Odometrie.
    // Merkt sich den groessten Fehler und den groessten Fehlersprung zwischen zwei Takten.
    void tick(double vl_set, double vr_set) {
        for (int k = 0; k < 10; ++k) {
            drive.step(dt_s, side_voltage(p, vl_set, drive.links_in_s()), side_voltage(p, vr_set, drive.rechts_in_s()));
            t_s += dt_s;
            left.step(drive.links_in() - l0);
            right.step(drive.rechts_in() - r0);
            l0 = drive.links_in();
            r0 = drive.rechts_in();
        }
        ++cycle;
        odo.update(left.measured_in(), right.measured_in(), imu_reading(), imu_valid());
        const Pose est = odo.pose();
        const sim::Pose truth = drive.pose();
        // Fehler = Schaetzung - Wahrheit; der Sprung ist seine Aenderung von einem Takt zum naechsten.
        const double err_x = est.x_in - truth.x_in, err_y = est.y_in - truth.y_in;
        const double err_h = wrap_deg(est.heading_deg - truth.h_deg);
        const double prev_err_x = prev_est.x_in - prev_truth.x_in, prev_err_y = prev_est.y_in - prev_truth.y_in;
        const double prev_err_h = wrap_deg(prev_est.heading_deg - prev_truth.h_deg);
        max_step_pos_in = std::max(max_step_pos_in, std::hypot(err_x - prev_err_x, err_y - prev_err_y));
        max_step_head_deg = std::max(max_step_head_deg, std::fabs(wrap_deg(err_h - prev_err_h)));
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
    sim::OdometryModel left;
    sim::OdometryModel right;
    sim::ImuModel imu;
    DriveOdometry odo;
    Fault fault = Fault::none;
    int fail_cycle = 0;
    double jump_deg = 30.0;
    double frozen_value = 0.0;
    int cycle = 0;
    double l0 = 0.0, r0 = 0.0, t_s = 0.0;
    Pose prev_est;
    sim::Pose prev_truth;
    double max_step_pos_in = 0.0, max_step_head_deg = 0.0;
    double max_pos_err_in = 0.0, max_head_err_deg = 0.0;
};

DriveOdometryParams odo_params() {
    DriveOdometryParams op;
    op.track_width_in = 12.0;
    op.jump_deg = 10.0;          // so viel darf IMU-Drehung je Takt von der Encoder-Drehung abweichen
    op.still_imu_deg = 0.05;     // IMU-Drehung je Takt darunter = steht
    op.still_encoder_deg = 0.2;  // Encoder-Drehung je Takt darueber = dreht
    op.still_cycles = 20;        // so viele Takte in Folge, bis auf Encoder umgeschaltet wird
    return op;
}

}  // namespace

TEST_CASE("Runner-Odometrie: Fahrt und Drehung ohne Fehler, IMU-Versatz faellt heraus, kein Ausfall") {
    Rig rig(sim::Pose{-10.0, 5.0, 100.0}, sim::OdometryParams{}, sim::ImuParams{}, 0.0, odo_params());
    rig.run(150, 30.0, 30.0);   // geradeaus
    rig.run(120, -10.0, 10.0);  // Punktdrehung ueber +-180 Grad hinweg
    rig.run(150, 20.0, 30.0);   // Bogen
    const sim::Pose truth = rig.drive.pose();
    CHECK_FALSE(rig.odo.degraded());
    CHECK(rig.max_pos_err_in < 0.05);
    CHECK(rig.max_head_err_deg < 0.1);
    CHECK(std::hypot(rig.odo.pose().x_in - truth.x_in, rig.odo.pose().y_in - truth.y_in) < 0.05);
}

TEST_CASE("Runner-Odometrie: Sim-Quadrat 3 m ohne Rauschen, Fehler <= 1 cm") {
    Rig rig(sim::Pose{-60.0, -60.0, 45.0}, sim::OdometryParams{}, sim::ImuParams{}, 0.0, odo_params());
    const double side_in = mm_to_in(3000.0);
    const double v = 40.0;
    const double turn_v = 10.0;
    for (int i = 0; i < 4; ++i) {
        const double lb = rig.drive.links_in(), rb = rig.drive.rechts_in();
        // Geradeaus, bis die mittlere Strecke erreicht ist (Regler mit Bremsrampe).
        for (int k = 0; k < 3000; ++k) {
            const double done = ((rig.drive.links_in() - lb) + (rig.drive.rechts_in() - rb)) / 2.0;
            const double err = side_in - done;
            if (std::abs(err) < 0.01 && std::abs(rig.drive.links_in_s()) < 0.05)
                break;
            const double sp = std::clamp(4.0 * err, -v, v);
            rig.tick(sp, sp);
        }
        const double al = rig.drive.links_in(), ar = rig.drive.rechts_in();
        const double quarter_arc_in = rig.p.spurbreite_in * pi / 4.0;
        for (int k = 0; k < 3000; ++k) {
            const double err = quarter_arc_in - ((rig.drive.rechts_in() - ar) - (rig.drive.links_in() - al)) / 2.0;
            if (std::abs(err) < 0.005 && std::abs(rig.drive.links_in_s()) < 0.05)
                break;
            const double sp = std::clamp(4.0 * err, -turn_v, turn_v);
            rig.tick(-sp, sp);
        }
    }
    MESSAGE("Quadrat: max. Positionsfehler ", rig.max_pos_err_in, " in, Heading ", rig.max_head_err_deg, " Grad");
    CHECK_FALSE(rig.odo.degraded());
    CHECK(rig.max_pos_err_in <= mm_to_in(10.0));
    CHECK(rig.max_head_err_deg < 0.5);
}

TEST_CASE("Runner-Odometrie: Rauschen, Drift und Skalierungsfehler loesen keinen falschen Ausfall aus") {
    sim::OdometryParams enc;
    enc.scale_error = 0.005;
    enc.sigma_in_per_tick = 0.0005;
    sim::ImuParams imu;
    imu.drift_deg_per_min = 2.0;
    imu.sigma_deg = 0.05;
    for (int seed_variant = 0; seed_variant < 3; ++seed_variant) {
        Rig rig(sim::Pose{0.0, 0.0, -150.0 + 10.0 * seed_variant}, enc, imu, 1.0, odo_params());
        rig.run(400, 20.0, 30.0);   // Kreis
        rig.run(200, -12.0, 12.0);  // Punktdrehung
        rig.run(200, 0.0, 0.0);     // Stand
        rig.run(200, 35.0, 35.0);   // geradeaus
        CHECK_FALSE(rig.odo.degraded());
    }
}

TEST_CASE("Runner-Odometrie: IMU-Sprung waehrend der Fahrt, Umschaltung ohne Posensprung") {
    Rig rig(sim::Pose{0.0, 0.0, 20.0}, sim::OdometryParams{}, sim::ImuParams{}, 0.0, odo_params());
    rig.fault = Fault::jump;
    rig.fail_cycle = 100;
    rig.run(99, 25.0, 25.0);
    CHECK_FALSE(rig.odo.degraded());
    rig.run(1, 25.0, 25.0);  // dieser Takt liefert den Sprung
    CHECK(rig.odo.degraded());
    CHECK(rig.odo.fallback_reason() == FallbackReason::imu_jump);
    rig.run(100, 20.0, 30.0);  // weiter im Bogen: Heading kommt jetzt von den Encodern
    CHECK(rig.max_step_pos_in < 0.05);
    CHECK(rig.max_step_head_deg < 0.1);
    CHECK(rig.max_head_err_deg < 0.1);
    CHECK(rig.max_pos_err_in < 0.1);
}

TEST_CASE("Runner-Odometrie: eingefrorenes IMU wird in einer Drehung erkannt, Fehler bleibt klein") {
    Rig rig(sim::Pose{0.0, 0.0, -90.0}, sim::OdometryParams{}, sim::ImuParams{}, 0.0, odo_params());
    rig.fault = Fault::frozen;
    rig.fail_cycle = 50;
    rig.run(50, 25.0, 25.0);  // Geradeausfahrt: ein eingefrorenes IMU waere hier harmlos und unsichtbar
    CHECK_FALSE(rig.odo.degraded());
    rig.run(100, -12.0, 12.0);  // Drehung um mehr als 90 Grad links
    CHECK(rig.odo.degraded());
    CHECK(rig.odo.fallback_reason() == FallbackReason::imu_frozen);
    rig.run(50, 25.0, 25.0);
    // Unterhalb von still_encoder_deg je Takt (Anlauf der Drehung) bleibt der Fehler unentdeckt: je Takt hoechstens diese
    // Schwelle, ueber den Anlauf einige Zehntel Grad.
    CHECK(rig.max_step_head_deg <= odo_params().still_encoder_deg + 0.01);
    CHECK(rig.max_head_err_deg < 1.0);
    CHECK(rig.max_pos_err_in < 0.5);
}

TEST_CASE("Runner-Odometrie: ungueltige Messung (HAL meldet Fehler) schaltet sofort um") {
    Rig rig(sim::Pose{0.0, 0.0, 0.0}, sim::OdometryParams{}, sim::ImuParams{}, 0.0, odo_params());
    rig.fault = Fault::invalid;
    rig.fail_cycle = 30;
    rig.run(29, 20.0, 30.0);
    CHECK_FALSE(rig.odo.degraded());
    rig.run(1, 20.0, 30.0);
    CHECK(rig.odo.degraded());
    CHECK(rig.odo.fallback_reason() == FallbackReason::imu_invalid);
    rig.run(100, 20.0, 30.0);
    CHECK(rig.max_step_head_deg < 0.1);
    CHECK(rig.max_head_err_deg < 0.1);
}

TEST_CASE("Runner-Odometrie: NaN vom IMU schaltet sofort um und die Pose bleibt endlich") {
    Rig rig(sim::Pose{0.0, 0.0, 0.0}, sim::OdometryParams{}, sim::ImuParams{}, 0.0, odo_params());
    rig.fault = Fault::non_finite;
    rig.fail_cycle = 30;
    rig.run(130, 20.0, 30.0);
    CHECK(rig.odo.degraded());
    CHECK(rig.odo.fallback_reason() == FallbackReason::imu_invalid);
    CHECK(std::isfinite(rig.odo.pose().x_in));
    CHECK(std::isfinite(rig.odo.pose().heading_deg));
    CHECK(rig.max_step_head_deg < 0.1);
}

TEST_CASE("Runner-Odometrie: Umschaltung ist endgueltig, auch wenn das IMU wieder plausibel meldet") {
    DriveOdometry odo(odo_params());
    odo.reset(Pose{}, 0.0, 0.0, 0.0);
    odo.update(0.0, 0.0, 0.0, false);
    REQUIRE(odo.degraded());
    for (int i = 1; i <= 10; ++i)
        odo.update(0.1 * i, 0.1 * i, 0.0, true);
    CHECK(odo.degraded());
    odo.reset(Pose{}, 0.0, 0.0, 0.0);  // neues Match
    CHECK_FALSE(odo.degraded());
}

TEST_CASE("Runner-Odometrie: IMU 179 -> -179 Grad ist eine Drehung um 2 Grad, kein Ausfall") {
    DriveOdometry odo(odo_params());
    odo.reset(Pose{0.0, 0.0, 179.0}, 0.0, 0.0, 179.0);
    // Punktdrehung um 2 Grad nach links bei 12 in Spur: jede Seite rollt 6 in * 2 Grad im Bogenmass.
    const double dr = deg_to_rad(2.0) * 12.0 / 2.0;
    odo.update(-dr, dr, -179.0, true);
    CHECK_FALSE(odo.degraded());
    CHECK(odo.pose().heading_deg == doctest::Approx(-179.0));
}

TEST_CASE("Runner-Odometrie: Stillstand-Erkennung zaehlt nur zusammenhaengende Takte") {
    DriveOdometry odo(odo_params());
    odo.reset(Pose{}, 0.0, 0.0, 0.0);
    const double half = deg_to_rad(1.0) * 12.0 / 2.0;  // Encoder zeigen 1 Grad pro Takt
    double l = 0.0, r = 0.0;
    for (int round = 0; round < 5; ++round) {
        for (int i = 0; i < 19; ++i) {  // 19 verdaechtige Takte (eine weniger als still_cycles), IMU steht
            l -= half;
            r += half;
            odo.update(l, r, 0.0, true);
        }
        l -= half;
        r += half;
        odo.update(l, r, 1.0, true);  // IMU dreht um 1 Grad: passt zu den Encodern
        CHECK_FALSE(odo.degraded());
        odo.reset(Pose{}, 0.0, 0.0, 0.0);
        l = r = 0.0;
    }
}
