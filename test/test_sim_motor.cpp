#include <cmath>

#include "doctest/doctest.h"
#include "hal/sim/sim_motor.hpp"

using frostbyte::hal::SimMotor;
using frostbyte::hal::SimMotorParams;

namespace {
// Testwerte, bewusst unabhängig von sim/sim_params.json.
SimMotorParams params() {
    SimMotorParams p;
    p.kS_V = 0.5;
    p.kV_V_per_deg_s = 0.01;
    p.kA_V_per_deg_s2 = 0.002;
    p.max_voltage_V = 12.0;
    return p;
}

void run(SimMotor& m, double seconds) {
    const double dt_s = 0.001;
    for (int i = 0; i < static_cast<int>(seconds / dt_s + 0.5); ++i)
        m.step(dt_s);
}
}  // namespace

TEST_CASE("SimMotor: Stillstand ohne Spannung") {
    SimMotor m(params());
    run(m, 0.5);
    CHECK(m.velocity_deg_s() == doctest::Approx(0.0));
    CHECK(m.position_deg() == doctest::Approx(0.0));
    CHECK(m.connected());
}

TEST_CASE("SimMotor: Enddrehzahl (V - kS) / kV") {
    SimMotor m(params());
    m.set_voltage_v(6.5);
    run(m, 2.0);
    CHECK(m.velocity_deg_s() == doctest::Approx(600.0).epsilon(0.001));
    CHECK(m.position_deg() > 0.0);
}

TEST_CASE("SimMotor: negative Spannung spiegelt die Antwort") {
    SimMotor a(params());
    SimMotor b(params());
    a.set_voltage_v(4.0);
    b.set_voltage_v(-4.0);
    run(a, 0.3);
    run(b, 0.3);
    CHECK(b.velocity_deg_s() == doctest::Approx(-a.velocity_deg_s()));
    CHECK(b.position_deg() == doctest::Approx(-a.position_deg()));
}

TEST_CASE("SimMotor: unterhalb kS bewegt sich nichts") {
    SimMotor m(params());
    m.set_voltage_v(0.4);
    run(m, 1.0);
    CHECK(m.velocity_deg_s() == doctest::Approx(0.0));
    CHECK(m.position_deg() == doctest::Approx(0.0));
}

TEST_CASE("SimMotor: Spannung wird auf +-max begrenzt") {
    SimMotor m(params());
    m.set_voltage_v(100.0);
    run(m, 2.0);
    CHECK(m.velocity_deg_s() == doctest::Approx((12.0 - 0.5) / 0.01).epsilon(0.001));
}

TEST_CASE("SimMotor: 63 % der Enddrehzahl nach einer Zeitkonstante kA/kV") {
    SimMotor m(params());
    m.set_voltage_v(6.5);
    run(m, 0.002 / 0.01);
    CHECK(m.velocity_deg_s() / 600.0 == doctest::Approx(1.0 - std::exp(-1.0)).epsilon(0.01));
}

TEST_CASE("SimMotor: stop() hält an und bleibt stehen") {
    SimMotor m(params());
    m.set_voltage_v(12.0);
    run(m, 1.0);
    REQUIRE(m.velocity_deg_s() > 100.0);
    m.stop();
    run(m, 1.0);
    CHECK(m.velocity_deg_s() == doctest::Approx(0.0));
    double pos = m.position_deg();
    run(m, 0.5);
    CHECK(m.position_deg() == doctest::Approx(pos));
}

TEST_CASE("SimMotor: Haftreibung lässt die Drehzahl nicht um null flattern") {
    SimMotor m(params());
    m.set_voltage_v(12.0);
    run(m, 1.0);
    m.set_voltage_v(0.0);
    for (int i = 0; i < 2000; ++i) {
        m.step(0.001);
        CHECK(m.velocity_deg_s() >= 0.0);
    }
    CHECK(m.velocity_deg_s() == doctest::Approx(0.0));
}

TEST_CASE("SimMotor: Ausfall wird gemeldet und Spannung ignoriert") {
    SimMotor m(params());
    m.set_connected(false);
    m.set_voltage_v(12.0);
    run(m, 0.5);
    CHECK_FALSE(m.connected());
    CHECK(m.position_deg() == doctest::Approx(0.0));
}
