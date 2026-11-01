#include <cmath>
#include <vector>

#include "doctest/doctest.h"
#include "sim/sensor_models.hpp"

using namespace frostbyte::sim;

namespace {

const std::string fixtures = FB_TEST_FIXTURES;

SensorParams alpha(const std::string& profile = "realistic") {
    return load_sensor_params(load_json_file(fixtures + "/sim_params_sensors.json"), "alpha", profile);
}

struct Stats {
    double mean = 0.0;
    double sigma = 0.0;
};

Stats stats_of(const std::vector<double>& v) {
    Stats s;
    for (double x : v)
        s.mean += x;
    s.mean /= static_cast<double>(v.size());
    for (double x : v)
        s.sigma += (x - s.mean) * (x - s.mean);
    s.sigma = std::sqrt(s.sigma / static_cast<double>(v.size() - 1));
    return s;
}

constexpr int samples = 1000;

// Echte relative Toleranz. doctest::Approx(x).epsilon(e) rechnet e * (1 + max(|ist|, |soll|)) und ist bei kleinen
// Werten fast absolut, also viel lockerer als e.
bool within_rel(double actual, double expected, double rel) { return std::fabs(actual - expected) <= rel * expected; }

}  // namespace

TEST_CASE("Sensorparameter: Tracking beim Stacker, Encoder beim Runner, Profil skaliert") {
    const Json j = load_json_file(fixtures + "/sim_params_sensors.json");
    const SensorParams a = load_sensor_params(j, "alpha", "harsh");
    CHECK(a.odometry_is_tracking);
    CHECK(a.odometry.scale_error == doctest::Approx(0.01));
    CHECK(a.gps.dropout_fraction == doctest::Approx(0.1));
    CHECK(a.noise_scale == doctest::Approx(2.0));
    const SensorParams b = load_sensor_params(j, "beta", "none");
    CHECK_FALSE(b.odometry_is_tracking);
    CHECK(b.odometry.sigma_in_per_tick == doctest::Approx(0.3));
    CHECK(b.noise_scale == doctest::Approx(0.0));
    CHECK_THROWS(load_sensor_params(j, "gamma", "none"));
    CHECK_THROWS(load_sensor_params(j, "alpha", "extrem"));
}

TEST_CASE("Gauss: Mittel 0, Sigma 1 (+-10 %) in 1000 Proben") {
    Rng rng(7);
    std::vector<double> v;
    for (int i = 0; i < samples; ++i)
        v.push_back(gauss(rng));
    const Stats s = stats_of(v);
    CHECK(std::fabs(s.mean) < 0.1);
    CHECK(within_rel(s.sigma, 1.0, 0.10));
}

TEST_CASE("Odometrie: Rauschen je Takt +-10 % der Vorgabe, Skalierungsfehler genau wie vorgegeben") {
    const SensorParams p = alpha();
    OdometryModel m(p.odometry, p.noise_scale, 11);
    CHECK(std::fabs(m.scale() - 1.0) == doctest::Approx(0.01));

    // Stillstand: nur Rauschen. Die Messdifferenz je Takt hat Sigma = sigma_in_pro_takt.
    std::vector<double> delta;
    double last = 0.0;
    for (int i = 0; i < samples; ++i) {
        m.step(0.0);
        delta.push_back(m.measured_in() - last);
        last = m.measured_in();
    }
    const Stats s = stats_of(delta);
    CHECK(within_rel(s.sigma, 0.1, 0.10));
    CHECK(std::fabs(s.mean) < 0.02);
}

TEST_CASE("Odometrie: 100 Zoll gefahren -> 100 * Skala (+- Rauschen)") {
    const SensorParams p = alpha();
    OdometryModel m(p.odometry, p.noise_scale, 3);
    for (int i = 0; i < 1000; ++i)
        m.step(0.1);
    // Rauschen summiert sich auf sqrt(1000) * 0.1 = 3,2 Zoll (1 Sigma); 5 Sigma Spielraum.
    CHECK(std::fabs(m.measured_in() - 100.0 * m.scale()) < 16.0);
    CHECK(m.scale() != doctest::Approx(1.0));
}

TEST_CASE("Odometrie: beide Vorzeichen des Skalierungsfehlers kommen vor") {
    const SensorParams p = alpha();
    bool plus = false;
    bool minus = false;
    for (std::uint64_t seed = 1; seed <= 40; ++seed) {
        OdometryModel m(p.odometry, p.noise_scale, seed);
        (m.scale() > 1.0 ? plus : minus) = true;
    }
    CHECK(plus);
    CHECK(minus);
}

TEST_CASE("IMU: Rauschen +-10 % der Vorgabe") {
    const SensorParams p = alpha();
    ImuModel m(p.imu, p.noise_scale, 5);
    std::vector<double> err;
    for (int i = 0; i < samples; ++i)
        err.push_back(m.measure_deg(30.0, 0.0));  // t = 0: noch keine Drift
    const Stats s = stats_of(err);
    CHECK(within_rel(s.sigma, 0.2, 0.10));
    CHECK(within_rel(s.mean, 30.0, 0.01));
}

TEST_CASE("IMU: Drift pro Minute genau wie vorgegeben, linear in der Zeit") {
    ImuParams p;
    p.drift_deg_per_min = 2.0;  // ohne Rauschen exakt prüfbar
    ImuModel m(p, 1.0, 9);
    CHECK(std::fabs(m.drift_deg_per_s()) * 60.0 == doctest::Approx(2.0));
    const double at_60 = m.measure_deg(10.0, 60.0) - 10.0;
    const double at_30 = m.measure_deg(10.0, 30.0) - 10.0;
    CHECK(std::fabs(at_60) == doctest::Approx(2.0));
    CHECK(at_30 == doctest::Approx(at_60 / 2.0));
}

TEST_CASE("IMU: Messwert bleibt in (-180, 180]") {
    ImuParams p;
    p.drift_deg_per_min = 0.0;
    ImuModel m(p, 1.0, 1);
    CHECK(m.measure_deg(190.0, 0.0) == doctest::Approx(-170.0));
    CHECK(m.measure_deg(-180.0, 0.0) == doctest::Approx(180.0));
    CHECK(m.measure_deg(720.0 + 45.0, 0.0) == doctest::Approx(45.0));
}

TEST_CASE("GPS: Positionsrauschen im Stand +-10 % der Vorgabe") {
    const SensorParams p = alpha();
    GpsModel m(p.gps, p.noise_scale, 21);
    std::vector<double> ex;
    while (static_cast<int>(ex.size()) < samples) {
        const GpsSample g = m.measure(10.0, -20.0, 0.0, 0.0);
        if (g.valid)
            ex.push_back(g.x_in - 10.0);
    }
    CHECK(within_rel(stats_of(ex).sigma, 1.0, 0.10));
}

TEST_CASE("GPS: Heading-Rauschen +-10 % der Vorgabe in 1000 Proben") {
    const SensorParams p = alpha();
    GpsModel m(p.gps, p.noise_scale, 24);
    std::vector<double> eh;
    while (static_cast<int>(eh.size()) < samples) {
        const GpsSample g = m.measure(0.0, 0.0, 30.0, 0.0);
        if (g.valid)
            eh.push_back(g.heading_deg - 30.0);
    }
    const Stats s = stats_of(eh);
    CHECK(within_rel(s.sigma, 2.0, 0.10));
    CHECK(std::fabs(s.mean) < 0.2);
}

TEST_CASE("GPS: Güte sinkt mit der Fahrgeschwindigkeit, Rauschen folgt der gemeldeten Güte") {
    const SensorParams p = alpha();
    GpsModel m(p.gps, p.noise_scale, 22);
    GpsSample still;
    while (!(still = m.measure(0, 0, 0, 0.0)).valid) {
    }
    CHECK(still.error_in == doctest::Approx(1.0));

    std::vector<double> ex;
    while (static_cast<int>(ex.size()) < samples) {
        const GpsSample g = m.measure(0.0, 0.0, 0.0, 20.0);  // 20 in/s -> Faktor 1 + 0,05 * 20 = 2
        if (g.valid) {
            CHECK(g.error_in == doctest::Approx(2.0));
            ex.push_back(g.x_in);
        }
    }
    CHECK(within_rel(stats_of(ex).sigma, 2.0, 0.10));
}

TEST_CASE("GPS: Aussetzeranteil +-10 % der Vorgabe in 1000 Proben") {
    // Eigener Wert 0,5: erwartet 500 Aussetzer, Streuung 16 (3,2 %), +-10 % sind rund 3 Sigma.
    GpsParams gp;
    gp.sigma_in = 1.0;
    gp.dropout_fraction = 0.5;
    GpsModel m(gp, 1.0, 23);
    int invalid = 0;
    for (int i = 0; i < samples; ++i)
        invalid += m.measure(0, 0, 0, 0).valid ? 0 : 1;
    CHECK(within_rel(static_cast<double>(invalid) / samples, 0.5, 0.10));
}

TEST_CASE("GPS: Aussetzer verschiebt die übrigen Messungen nicht") {
    GpsParams with;
    with.sigma_in = 1.0;
    with.dropout_fraction = 0.5;
    GpsParams without = with;
    without.dropout_fraction = 0.0;
    GpsModel a(with, 1.0, 4);
    GpsModel b(without, 1.0, 4);
    int compared = 0;
    for (int i = 0; i < 200; ++i) {
        const GpsSample ga = a.measure(0, 0, 0, 0);
        const GpsSample gb = b.measure(0, 0, 0, 0);
        if (ga.valid) {
            CHECK(ga.x_in == gb.x_in);
            ++compared;
        }
    }
    CHECK(compared > 50);
}

TEST_CASE("Profil none: perfekte Sensoren") {
    const SensorParams p = alpha("none");
    OdometryModel o(p.odometry, p.noise_scale, 1);
    o.step(5.0);
    CHECK(o.measured_in() == 5.0);
    ImuModel i(p.imu, p.noise_scale, 1);
    CHECK(i.measure_deg(12.5, 50.0) == 12.5);
    GpsModel g(p.gps, p.noise_scale, 1);
    for (int k = 0; k < 500; ++k) {
        const GpsSample s = g.measure(1.0, 2.0, 3.0, 30.0);
        REQUIRE(s.valid);
        CHECK(s.x_in == 1.0);
        CHECK(s.heading_deg == 3.0);
    }
}

TEST_CASE("Determinismus: gleicher Seed = gleiche Messwerte, anderer Seed = andere") {
    const SensorParams p = alpha();
    auto run = [&](std::uint64_t seed) {
        OdometryModel o(p.odometry, p.noise_scale, seed);
        ImuModel i(p.imu, p.noise_scale, seed);
        GpsModel g(p.gps, p.noise_scale, seed);
        double acc = 0.0;
        for (int k = 0; k < 100; ++k) {
            o.step(0.5);
            const GpsSample s = g.measure(1, 1, 1, 5);
            acc += o.measured_in() + i.measure_deg(10.0, k * 0.01) + s.x_in;
        }
        return acc;
    };
    CHECK(run(5) == run(5));
    CHECK(run(5) != run(6));
}

TEST_CASE("Ströme: Modelle beeinflussen sich nicht") {
    const SensorParams p = alpha();
    OdometryModel o1(p.odometry, p.noise_scale, 8);
    OdometryModel o2(p.odometry, p.noise_scale, 8);
    ImuModel imu(p.imu, p.noise_scale, 8);
    o1.step(1.0);
    imu.measure_deg(0.0, 0.0);  // zusätzlicher Zufallsverbrauch darf o2 nicht verändern
    o2.step(1.0);
    CHECK(o1.measured_in() == o2.measured_in());
}

TEST_CASE("sim_params.json: beide Roboter lassen sich laden (nur Struktur, keine Werte)") {
    const Json j = load_json_file(std::string(FB_ROOT_DIR) + "/sim/sim_params.json");
    for (const char* robot : {"stacker", "runner"})
        for (const char* profile : {"none", "realistic", "harsh"})
            CHECK_NOTHROW(load_sensor_params(j, robot, profile));
    CHECK(load_sensor_params(j, "stacker", "none").odometry_is_tracking);
    CHECK_FALSE(load_sensor_params(j, "runner", "none").odometry_is_tracking);
}
