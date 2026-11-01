#include <algorithm>
#include <cmath>

#include "core/units.hpp"
#include "doctest/doctest.h"
#include "estimation/odometry.hpp"
#include "sim/drive.hpp"
#include "sim/sensor_models.hpp"

using namespace frostbyte;

namespace {

// Testwerte, bewusst verschieden von sim/sim_params.json.
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

// Beide Raeder abseits der Drehmitte, damit die Versatzkorrektur wirklich gebraucht wird.
constexpr double parallel_y_in = 2.5;       // Parallelrad links der Mitte
constexpr double perpendicular_x_in = -3.0;  // Querrad hinter der Mitte
constexpr double imu_offset_deg = 37.0;      // Einbaulage des IMU gegenueber dem Feld
constexpr double dt_s = 0.001;

double side_voltage(const sim::DriveParams& p, double v_set, double v_ist) {
    const double sign = v_set > 0.0 ? 1.0 : (v_set < 0.0 ? -1.0 : 0.0);
    return p.kS_V * sign + p.kV_V_per_in_s * v_set + 1.0 * (v_set - v_ist);
}

// Sensoren wie am Roboter, aber nur aus den Seitenwegen des Fahrmodells (eigene Kinematik, nicht die der Odometrie):
// dtheta = (dr - dl) / Spurbreite; Parallelrad = Mitte - dtheta * y_w; Querrad = dtheta * x_w (kein Seitenschlupf).
struct Rig {
    Rig(const sim::Pose& start, double imu_off_deg)
        : p(drive_params()),
          drive(p, start),
          par(sim::OdometryParams{}, 0.0, 1),
          perp(sim::OdometryParams{}, 0.0, 2),
          imu(sim::ImuParams{}, 0.0, 3),
          imu_off(imu_off_deg),
          odo(TrackingGeometry{parallel_y_in, perpendicular_x_in}) {
        l0 = drive.links_in();
        r0 = drive.rechts_in();
        sample();
        odo.reset(Pose{start.x_in, start.y_in, start.h_deg}, par.measured_in(), perp.measured_in(), imu_deg());
    }

    double imu_deg() { return wrap_deg(imu.measure_deg(drive.pose().h_deg, t_s) + imu_off); }

    void sample() {
        const double l = drive.links_in(), r = drive.rechts_in();
        const double dl = l - l0, dr = r - r0;
        l0 = l;
        r0 = r;
        const double dtheta_rad = (dr - dl) / p.spurbreite_in;
        par.step((dl + dr) / 2.0 - dtheta_rad * parallel_y_in);
        perp.step(dtheta_rad * perpendicular_x_in);
    }

    // Ein Steuertakt (10 ms) Physik mit gleichbleibenden Seitentempi, danach Odometrie; merkt sich den groessten Fehler.
    void tick(double vl_set, double vr_set) {
        for (int k = 0; k < 10; ++k) {
            drive.step(dt_s, side_voltage(p, vl_set, drive.links_in_s()), side_voltage(p, vr_set, drive.rechts_in_s()));
            t_s += dt_s;
            sample();
        }
        odo.update(par.measured_in(), perp.measured_in(), imu_deg());
        const sim::Pose truth = drive.pose();
        const Pose est = odo.pose();
        max_pos_err_in = std::max(max_pos_err_in, std::hypot(est.x_in - truth.x_in, est.y_in - truth.y_in));
        max_head_err_deg = std::max(max_head_err_deg, std::fabs(wrap_deg(est.heading_deg - truth.h_deg)));
    }

    // Relatives Ziel aus den Seitenwegen (wie der Testregler in test_sim_drive.cpp).
    void move(double fwd_in, double arc_in) {
        const double lb = drive.links_in(), rb = drive.rechts_in();
        for (int i = 0; i < 3000; ++i) {
            const double dl = drive.links_in() - lb, dr = drive.rechts_in() - rb;
            const double err_fwd = fwd_in - (dl + dr) / 2.0;
            const double err_arc = arc_in - (dr - dl) / 2.0;
            if (std::abs(err_fwd) < 0.01 && std::abs(err_arc) < 0.005 && std::abs(drive.links_in_s()) < 0.05 &&
                std::abs(drive.rechts_in_s()) < 0.05)
                return;
            const double v_fwd = std::clamp(4.0 * err_fwd, -40.0, 40.0);
            const double v_arc = std::clamp(4.0 * err_arc, -20.0, 20.0);
            tick(v_fwd - v_arc, v_fwd + v_arc);
        }
        FAIL("Testregler erreicht das Ziel nicht");
    }

    sim::DriveParams p;
    sim::DriveModel drive;
    sim::OdometryModel par;
    sim::OdometryModel perp;
    sim::ImuModel imu;
    double imu_off;
    TrackingOdometry odo;
    double l0 = 0.0, r0 = 0.0, t_s = 0.0;
    double max_pos_err_in = 0.0;
    double max_head_err_deg = 0.0;
};

}  // namespace

TEST_CASE("Odometrie: Punktdrehung mit versetzten Raedern laesst die Position stehen") {
    Rig rig(sim::Pose{10.0, -20.0, 0.0}, imu_offset_deg);
    rig.move(0.0, 12.0 * pi / 4.0);  // 90 Grad links auf der Stelle
    CHECK(rig.odo.pose().heading_deg == doctest::Approx(90.0).epsilon(0.001));
    CHECK(rig.odo.pose().x_in == doctest::Approx(10.0).epsilon(0.002));
    CHECK(rig.odo.pose().y_in == doctest::Approx(-20.0).epsilon(0.002));
    CHECK(rig.max_pos_err_in < 0.05);
}

TEST_CASE("Odometrie: Wegsummen ohne Aenderung bewegen die Pose nicht, Heading folgt dem IMU-Versatz") {
    TrackingOdometry odo(TrackingGeometry{parallel_y_in, perpendicular_x_in});
    odo.reset(Pose{1.0, 2.0, 120.0}, 55.0, -4.0, 10.0);
    odo.update(55.0, -4.0, 10.0);
    CHECK(odo.pose().x_in == 1.0);
    CHECK(odo.pose().y_in == 2.0);
    CHECK(odo.pose().heading_deg == doctest::Approx(120.0));
    odo.update(55.0, -4.0, 10.0 + 70.0);  // IMU dreht 70 Grad: Heading = 120 + 70 = 190 -> -170
    CHECK(odo.pose().heading_deg == doctest::Approx(-170.0));
}

TEST_CASE("Odometrie: Sim-Quadrat 3 m ohne Rauschen, Fehler <= 1 cm ueber den ganzen Lauf") {
    // Startheading 45 Grad und IMU-Versatz: die vier Drehungen laufen ueber +-180 Grad.
    Rig rig(sim::Pose{-60.0, -60.0, 45.0}, imu_offset_deg);
    const double side_in = mm_to_in(3000.0);
    const double quarter_arc_in = rig.p.spurbreite_in * pi / 4.0;
    for (int i = 0; i < 4; ++i) {
        rig.move(side_in, 0.0);
        rig.move(0.0, quarter_arc_in);
    }
    CHECK(rig.max_pos_err_in <= mm_to_in(10.0));
    CHECK(rig.max_head_err_deg < 0.5);
    const sim::Pose truth = rig.drive.pose();
    CHECK(std::hypot(rig.odo.pose().x_in - truth.x_in, rig.odo.pose().y_in - truth.y_in) <= mm_to_in(10.0));
    // Das Quadrat schliesst: Endpunkt = Startpunkt (Regler nutzt Seitenwege, daher nur grob).
    CHECK(std::hypot(truth.x_in + 60.0, truth.y_in + 60.0) < 1.0);
}

TEST_CASE("Odometrie: IMU springt von 179 auf -179 Grad, das ist eine Drehung um 2 Grad, nicht um -358") {
    TrackingOdometry odo(TrackingGeometry{});
    odo.reset(Pose{0.0, 0.0, 179.0}, 0.0, 0.0, 179.0);
    odo.update(1.0, 0.0, -179.0);  // 1 Zoll geradeaus waehrend 2 Grad Linksdrehung, Blick nach -x
    CHECK(odo.pose().heading_deg == doctest::Approx(-179.0));
    CHECK(odo.pose().x_in == doctest::Approx(-1.0).epsilon(0.001));
    CHECK(odo.pose().y_in == doctest::Approx(0.0).epsilon(0.001).scale(1.0));
}
