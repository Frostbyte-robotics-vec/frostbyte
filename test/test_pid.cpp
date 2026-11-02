#include <cmath>
#include <limits>

#include "control/pid.hpp"
#include "doctest/doctest.h"

using namespace frostbyte;

namespace {

// Testwerte, unabhängig von config/.
PidGains gains(double kp, double ki, double kd) {
    PidGains g;
    g.kp = kp;
    g.ki = ki;
    g.kd = kd;
    g.i_zone = 0.0;  // 0 = überall integrieren
    g.i_max_v = 100.0;
    g.out_max_v = 100.0;
    return g;
}

}  // namespace

TEST_CASE("PID: P-Anteil ist kp * Fehler, Ausgabe wird begrenzt") {
    PidGains g = gains(2.0, 0.0, 0.0);
    g.out_max_v = 12.0;
    Pid pid(g);
    CHECK(pid.update(3.0, 0.01) == doctest::Approx(6.0));
    CHECK(pid.update(-4.0, 0.01) == doctest::Approx(-8.0));
    CHECK(pid.update(10.0, 0.01) == doctest::Approx(12.0));
    CHECK(pid.update(-10.0, 0.01) == doctest::Approx(-12.0));
}

TEST_CASE("PID: I-Anteil summiert Fehler mal Zeit") {
    Pid pid(gains(0.0, 1.0, 0.0));
    double out = 0.0;
    for (int i = 0; i < 100; ++i)
        out = pid.update(2.0, 0.01);  // 1 s lang Fehler 2 -> Integral 2
    CHECK(out == doctest::Approx(2.0));
}

TEST_CASE("PID: D-Anteil ohne Sprung beim ersten Aufruf, danach kd * Fehleränderung / dt") {
    Pid pid(gains(0.0, 0.0, 0.5));
    CHECK(pid.update(10.0, 0.01) == doctest::Approx(0.0));
    CHECK(pid.update(9.0, 0.01) == doctest::Approx(0.5 * (9.0 - 10.0) / 0.01));
}

TEST_CASE("PID: dt <= 0 rechnet nur den P-Anteil") {
    Pid pid(gains(1.0, 1.0, 1.0));
    pid.update(5.0, 0.01);
    CHECK(pid.update(3.0, 0.0) == doctest::Approx(3.0 + 1.0 * 5.0 * 0.01));  // Integral bleibt, kein D
    CHECK(pid.update(3.0, -0.01) == doctest::Approx(3.0 + 1.0 * 5.0 * 0.01));
}

TEST_CASE("PID: Anti-Windup – I-Anteil auf i_max_v begrenzt, keine Integration bei Sättigung in Fehlerrichtung") {
    PidGains g = gains(1.0, 5.0, 0.0);
    g.i_max_v = 2.0;
    g.out_max_v = 12.0;
    Pid pid(g);
    for (int i = 0; i < 1000; ++i)  // 10 s gesättigt
        pid.update(20.0, 0.01);
    // Fehler kehrt um: das Integral ist während der Sättigung nicht gewachsen (sonst +2 V aus dem I-Anteil),
    // also nur P -1 V plus ein I-Schritt 5 * -1 * 0,01.
    CHECK(pid.update(-1.0, 0.01) == doctest::Approx(-1.05));
    // Begrenzung: nie mehr als i_max_v aus dem I-Anteil.
    PidGains gi = gains(0.0, 5.0, 0.0);
    gi.i_max_v = 2.0;
    Pid limited(gi);
    double o = 0.0;
    for (int i = 0; i < 1000; ++i)
        o = limited.update(1.0, 0.01);
    CHECK(o == doctest::Approx(2.0));
}

TEST_CASE("PID: Integration nur innerhalb von i_zone") {
    PidGains g = gains(0.0, 1.0, 0.0);
    g.i_zone = 5.0;
    Pid pid(g);
    for (int i = 0; i < 100; ++i)
        pid.update(10.0, 0.01);  // außerhalb: nichts
    CHECK(pid.update(10.0, 0.01) == doctest::Approx(0.0));
    for (int i = 0; i < 100; ++i)
        pid.update(4.0, 0.01);
    CHECK(pid.update(4.0, 0.0) == doctest::Approx(4.0));
}

TEST_CASE("PID: reset löscht Integral und D-Gedächtnis") {
    Pid pid(gains(0.0, 1.0, 1.0));
    for (int i = 0; i < 50; ++i)
        pid.update(2.0, 0.01);
    pid.reset();
    CHECK(pid.update(7.0, 0.01) == doctest::Approx(7.0 * 0.01));  // nur der eine I-Schritt, kein D-Sprung
}

TEST_CASE("PID: ungültige Fehlerwerte geben 0 und verderben den Zustand nicht") {
    Pid pid(gains(1.0, 1.0, 1.0));
    CHECK(pid.update(std::numeric_limits<double>::quiet_NaN(), 0.01) == 0.0);
    CHECK(pid.update(std::numeric_limits<double>::infinity(), 0.01) == 0.0);
    CHECK(std::isfinite(pid.update(1.0, 0.01)));
}
