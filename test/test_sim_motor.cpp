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
    p.resistance_ohm = 4.0;
    p.max_current_A = 2.5;
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

TEST_CASE("SimMotor: Strom ohne Spannung und im Stillstand ist null") {
    SimMotor m(params());
    CHECK(m.current_a() == doctest::Approx(0.0));
    run(m, 0.1);
    CHECK(m.current_a() == doctest::Approx(0.0));
}

TEST_CASE("SimMotor: blockiert (Stall) zieht viel Strom, frei laufend wenig") {
    SimMotor free_run(params());
    free_run.set_voltage_v(6.5);
    run(free_run, 2.0);  // Enddrehzahl erreicht, Gegenspannung kV*v

    SimMotor stalled(params());
    stalled.set_blocked(true);
    stalled.set_voltage_v(6.5);
    run(stalled, 0.5);

    CHECK(stalled.velocity_deg_s() == doctest::Approx(0.0));
    CHECK(stalled.current_a() == doctest::Approx(6.5 / 4.0));                 // V / R, unter der Grenze
    CHECK(free_run.current_a() == doctest::Approx(0.5 / 4.0).epsilon(0.01));  // kS / R
    CHECK(stalled.current_a() > 5.0 * free_run.current_a());
}

TEST_CASE("SimMotor: Strom ist ein Betrag und auf max_current_A begrenzt") {
    SimMotor a(params());
    SimMotor b(params());
    a.set_blocked(true);
    b.set_blocked(true);
    a.set_voltage_v(12.0);
    b.set_voltage_v(-12.0);
    CHECK(a.current_a() == doctest::Approx(2.5));  // 12 / 4 = 3 A, begrenzt
    CHECK(b.current_a() == doctest::Approx(a.current_a()));
}

TEST_CASE("SimMotor: Bremse und Ausfall ziehen keinen Strom") {
    SimMotor m(params());
    m.set_blocked(true);
    m.set_voltage_v(8.0);
    CHECK(m.current_a() > 0.0);
    m.set_connected(false);
    CHECK(m.current_a() == doctest::Approx(0.0));
    m.set_connected(true);
    m.stop();
    CHECK(m.current_a() == doctest::Approx(0.0));
}
