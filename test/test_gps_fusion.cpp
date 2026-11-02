#include <cmath>

#include "core/units.hpp"
#include "doctest/doctest.h"
#include "estimation/drive_odometry.hpp"
#include "estimation/gps_fusion.hpp"
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

GpsFusionParams fusion_params() {
    GpsFusionParams p;
    p.max_error_in = 1.0;     // Messungen mit schlechterer gemeldeter Güte zählen nicht
    p.max_speed_in_s = 20.0;  // schneller als das: GPS wird ignoriert
    p.gate_in = 6.0;          // weiter weg von der Schätzung: Ausreißer
    p.position_gain = 0.1;
    p.heading_gain = 0.0;
    p.resync_samples = 10;  // so viele Ausreisser in Folge gelten als Wahrheit (Odometrie ist verrutscht)
    return p;
}

DriveOdometryParams odo_params() {
    DriveOdometryParams op;
    op.track_width_in = 12.0;
    op.jump_deg = 10.0;
    op.still_imu_deg = 0.05;
    op.still_encoder_deg = 0.2;
    op.still_cycles = 20;
    return op;
}

sim::GpsParams gps_params() {
    sim::GpsParams g;
    g.sigma_in = 0.3;
    g.sigma_deg = 1.0;
    g.dropout_fraction = 0.05;
    g.quality_loss_per_in_s = 0.02;
    return g;
}

constexpr double dt_s = 0.001;
constexpr double encoder_scale_error = 0.02;  // 2 % Wegfehler: auf 100 in rund 2 in Drift

double side_voltage(const sim::DriveParams& p, double v_set, double v_ist) {
    const double sign = v_set > 0.0 ? 1.0 : (v_set < 0.0 ? -1.0 : 0.0);
    return p.kS_V * sign + p.kV_V_per_in_s * v_set + 1.0 * (v_set - v_ist);
}

GpsReading to_reading(const sim::GpsSample& s) {
    return GpsReading{s.valid, s.x_in, s.y_in, s.heading_deg, s.error_in};
}

// Geradeausfahrt mit Encodern, die 2 % zu viel (oder zu wenig, je nach Seed) messen; GPS alle 5 Takte.
struct Run {
    Run(std::uint64_t seed, const GpsFusionParams& fp)
        : p(drive_params()),
          drive(p, sim::Pose{-50.0, 0.0, 0.0}),
          left(sim::OdometryParams{encoder_scale_error, 0.0}, 1.0, seed),
          right(sim::OdometryParams{encoder_scale_error, 0.0}, 1.0, seed),  // gleicher Seed = gleiches Vorzeichen
          gps(gps_params(), 1.0, seed + 2),
          odo(odo_params()),
          fusion(fp) {
        odo.reset(Pose{-50.0, 0.0, 0.0}, 0.0, 0.0, 0.0);
        fusion.reset();
        fused = odo.pose();
    }

    void drive_cycles(int cycles, double v_in_s, bool with_gps = true) {
        for (int c = 0; c < cycles; ++c) {
            for (int k = 0; k < 10; ++k) {
                const double l0 = drive.links_in(), r0 = drive.rechts_in();
                drive.step(dt_s, side_voltage(p, v_in_s, drive.links_in_s()),
                           side_voltage(p, v_in_s, drive.rechts_in_s()));
                left.step(drive.links_in() - l0);
                right.step(drive.rechts_in() - r0);
            }
            ++cycle;
            const Pose before = odo.pose();
            odo.update(left.measured_in(), right.measured_in(), drive.pose().h_deg, true);
            // Tempo aus der Odometrie, nicht aus der Wahrheit: so sieht es auch der Roboter.
            const double speed = std::hypot(odo.pose().x_in - before.x_in, odo.pose().y_in - before.y_in) / 0.01;
            GpsReading reading;  // valid = false: kein GPS in diesem Takt
            if (with_gps && cycle % 5 == 0) {
                const sim::Pose t = drive.pose();
                reading = to_reading(gps.measure(t.x_in, t.y_in, t.h_deg, speed));
            }
            fused = fusion.update(odo.pose(), reading, speed);
        }
    }

    double odo_error_in() const {
        const sim::Pose t = drive.pose();
        return std::hypot(odo.pose().x_in - t.x_in, odo.pose().y_in - t.y_in);
    }
    double fused_error_in() const {
        const sim::Pose t = drive.pose();
        return std::hypot(fused.x_in - t.x_in, fused.y_in - t.y_in);
    }

    sim::DriveParams p;
    sim::DriveModel drive;
    sim::OdometryModel left, right;
    sim::GpsModel gps;
    DriveOdometry odo;
    GpsFusion fusion;
    Pose fused;
    int cycle = 0;
};

}  // namespace

TEST_CASE("GPS-Fusion: 2 Zoll Odometrie-Drift werden auf <= 1 Zoll zurueckgezogen") {
    for (std::uint64_t seed : {1u, 2u, 3u, 4u}) {
        Run ohne(seed, fusion_params());
        ohne.drive_cycles(1000, 10.0, false);  // 10 s * 10 in/s = 100 in
        REQUIRE(ohne.odo_error_in() > 1.5);    // der Drift ist wirklich da

        Run mit(seed, fusion_params());
        mit.drive_cycles(1000, 10.0);
        MESSAGE("Seed ", seed, ": Odometrie ", mit.odo_error_in(), " in, fusioniert ", mit.fused_error_in(), " in");
        CHECK(mit.fused_error_in() <= 1.0);
        CHECK(mit.odo_error_in() > 1.5);  // die Odometrie selbst bleibt unveraendert
    }
}

TEST_CASE("GPS-Fusion: Drift erst aufbauen, dann mit GPS zurueckziehen") {
    Run r(1, fusion_params());
    r.drive_cycles(1000, 10.0, false);  // 100 in ohne GPS: ca. 2 in Drift
    REQUIRE(r.fused_error_in() > 1.5);
    int cycles = 0;
    while (r.fused_error_in() > 1.0 && cycles < 2000) {
        r.drive_cycles(1, 5.0);  // langsam weiter, GPS an
        ++cycles;
    }
    MESSAGE("Zurueckgezogen nach ", cycles, " Takten (", cycles / 100.0, " s), Gewinn 0.1, GPS alle 5 Takte");
    CHECK(cycles < 1000);
    CHECK(r.fused_error_in() <= 1.0);
}

TEST_CASE("GPS-Fusion: ein einzelner Ausreisser wird verworfen, viele in Folge setzen die Pose neu") {
    GpsFusion f(fusion_params());
    f.reset();
    const Pose odo{0.0, 0.0, 0.0};
    const GpsReading far{true, 10.0, 0.0, 0.0, 0.3};  // Odometrie 10 in neben dem GPS, Tor 6 in
    for (int i = 0; i < 9; ++i) {
        f.update(odo, far, 0.0);
        CHECK(f.last_result() == GpsResult::outlier);
    }
    f.update(odo, GpsReading{true, 0.2, 0.0, 0.0, 0.3}, 0.0);  // ein guter Wert unterbricht die Folge
    CHECK(f.last_result() == GpsResult::accepted);
    for (int i = 0; i < 9; ++i)
        f.update(odo, far, 0.0);
    CHECK(f.last_result() == GpsResult::outlier);  // Zaehler war zurueckgesetzt: erst 9 in Folge

    // Odometrie verrutscht im Lauf (z. B. Schlupf nach Kollision) um 10 in: GPS holt die Pose wieder.
    Run r(1, fusion_params());
    r.drive_cycles(300, 10.0);
    r.odo.reset(Pose{r.odo.pose().x_in + 10.0, r.odo.pose().y_in, r.odo.pose().heading_deg}, r.left.measured_in(),
                r.right.measured_in(), r.drive.pose().h_deg);
    r.drive_cycles(1, 10.0, false);
    REQUIRE(r.fused_error_in() > 8.0);
    r.drive_cycles(1000, 10.0);
    MESSAGE("Nach Verrutschen um 10 in: Fehler ", r.fused_error_in(), " in");
    CHECK(r.fused_error_in() <= 1.0);
}

TEST_CASE("GPS-Fusion: zu schnelle Fahrt wird ignoriert") {
    Run r(3, fusion_params());
    r.drive_cycles(100, 30.0, false);  // erst auf Tempo kommen (ueber max_speed_in_s), ohne GPS
    r.drive_cycles(200, 30.0);
    CHECK(r.fusion.last_result() == GpsResult::too_fast);
    CHECK(r.fused_error_in() == doctest::Approx(r.odo_error_in()));
}

TEST_CASE("GPS-Fusion: schlechte gemeldete Guete, fehlende Messung und Ausreisser werden verworfen") {
    GpsFusion f(fusion_params());
    f.reset();
    const Pose odo{10.0, 20.0, 30.0};

    f.update(odo, GpsReading{true, 10.4, 20.0, 30.0, 2.5}, 5.0);
    CHECK(f.last_result() == GpsResult::bad_quality);
    CHECK(f.pose().x_in == 10.0);

    f.update(odo, GpsReading{false, 0.0, 0.0, 0.0, 0.0}, 5.0);
    CHECK(f.last_result() == GpsResult::no_data);
    CHECK(f.pose().x_in == 10.0);

    f.update(odo, GpsReading{true, 40.0, 20.0, 30.0, 0.3}, 5.0);  // 30 in daneben, Tor 6 in
    CHECK(f.last_result() == GpsResult::outlier);
    CHECK(f.pose().x_in == 10.0);

    f.update(odo, GpsReading{true, 11.0, 20.0, 30.0, 0.3}, 5.0);
    CHECK(f.last_result() == GpsResult::accepted);
    CHECK(f.pose().x_in == doctest::Approx(10.1));  // Gewinn 0.1 * 1 in
    CHECK(f.pose().y_in == doctest::Approx(20.0));
}

TEST_CASE("GPS-Fusion: nicht endliche Messwerte werden verworfen") {
    GpsFusion f(fusion_params());
    f.reset();
    f.update(Pose{0.0, 0.0, 0.0}, GpsReading{true, std::nan(""), 0.0, 0.0, 0.3}, 0.0);
    CHECK(f.last_result() == GpsResult::bad_quality);
    CHECK(std::isfinite(f.pose().x_in));
}

TEST_CASE("GPS-Fusion: Heading nur mit heading_gain > 0, Wrap bei +-180 Grad") {
    GpsFusion a(fusion_params());
    a.reset();
    for (int i = 0; i < 100; ++i)
        a.update(Pose{0.0, 0.0, 0.0}, GpsReading{true, 0.0, 0.0, 5.0, 0.3}, 0.0);
    CHECK(a.pose().heading_deg == doctest::Approx(0.0));

    GpsFusionParams on = fusion_params();
    on.heading_gain = 0.1;
    GpsFusion b(on);
    b.reset();
    for (int i = 0; i < 100; ++i)
        b.update(Pose{0.0, 0.0, 179.0}, GpsReading{true, 0.0, 0.0, -176.0, 0.3}, 0.0);  // GPS: 184 = -176
    CHECK(std::fabs(wrap_deg(b.pose().heading_deg - (-176.0))) < 0.1);
}

TEST_CASE("GPS-Fusion: reset loescht die Korrektur") {
    GpsFusion f(fusion_params());
    f.reset();
    f.update(Pose{0.0, 0.0, 0.0}, GpsReading{true, 2.0, 0.0, 0.0, 0.3}, 0.0);
    REQUIRE(f.pose().x_in > 0.0);
    f.reset();
    CHECK(f.update(Pose{5.0, 5.0, 0.0}, GpsReading{}, 0.0).x_in == 5.0);
}
