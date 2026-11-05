#include <cmath>
#include <cstring>
#include <string>

#include "doctest/doctest.h"
#include "util/port_check.hpp"

using namespace frostbyte;

namespace {
// Negativer Port = umgekehrt eingetragen (wie in config/); geprüft wird mit dem Betrag.
const PortExpect kExpect[] = {
    {1, DeviceKind::motor, "Antrieb L", "Rad vorwaerts"},
    {-2, DeviceKind::motor, "Antrieb R", "Rad vorwaerts"},
    {5, DeviceKind::imu, "IMU", nullptr},
};

PortCheck all_found() {
    PortCheck pc(kExpect, 3);
    pc.record_found(1, DeviceKind::motor);
    pc.record_found(2, DeviceKind::motor);
    pc.record_found(5, DeviceKind::imu);
    return pc;
}
}  // namespace

TEST_CASE("pulse_moved: Betrag zaehlt, nicht das Vorzeichen") {
    CHECK(pulse_moved(12.0, 5.0));
    CHECK(pulse_moved(-12.0, 5.0));
    CHECK_FALSE(pulse_moved(4.9, 5.0));
    CHECK_FALSE(pulse_moved(-4.9, 5.0));
    CHECK_FALSE(pulse_moved(std::nan(""), 5.0));
}

TEST_CASE("PortCheck: alles bestaetigt -> keine Probleme") {
    PortCheck pc = all_found();
    pc.record_pulse(1, 30.0);
    pc.record_pulse(2, 30.0);
    pc.record_confirm(1, true);
    pc.record_confirm(2, true);
    CHECK(pc.verdict(1) == PortVerdict::ok);
    CHECK(pc.verdict(2) == PortVerdict::ok);
    CHECK(pc.verdict(5) == PortVerdict::ok);
    CHECK(pc.problem_count() == 0);
    CHECK(pc.pending_count() == 0);
}

TEST_CASE("PortCheck: negativer Port in der Erwartung wird mit dem Betrag gefunden") {
    PortCheck pc = all_found();
    CHECK(pc.verdict(2) == PortVerdict::not_pulsed);  // nicht "unexpected"
}

TEST_CASE("PortCheck: fehlendes, falsches und unerwartetes Geraet") {
    PortCheck pc(kExpect, 3);
    pc.record_found(1, DeviceKind::none);
    pc.record_found(2, DeviceKind::rotation);
    pc.record_found(5, DeviceKind::imu);
    pc.record_found(9, DeviceKind::motor);  // nicht in der Config
    CHECK(pc.verdict(1) == PortVerdict::missing);
    CHECK(pc.verdict(2) == PortVerdict::wrong_type);
    CHECK(pc.verdict(9) == PortVerdict::unexpected);
    CHECK(pc.verdict(10) == PortVerdict::ok);  // leer und nicht erwartet
    CHECK(pc.problem_count() == 3);
}

TEST_CASE("PortCheck: Drehrichtung nur nach Bestaetigung durch den Menschen") {
    PortCheck pc = all_found();
    CHECK(pc.verdict(1) == PortVerdict::not_pulsed);
    pc.record_pulse(1, -30.0);  // das Vorzeichen des Encoders sagt nichts ueber die Mechanik
    CHECK(pc.verdict(1) == PortVerdict::unconfirmed);
    pc.record_confirm(1, false);
    CHECK(pc.verdict(1) == PortVerdict::wrong_direction);
    pc.record_confirm(1, true);  // erneuter Versuch ueberschreibt
    CHECK(pc.verdict(1) == PortVerdict::ok);
}

TEST_CASE("PortCheck: dreht nicht, Bestaetigung ohne Puls zaehlt nicht") {
    PortCheck pc = all_found();
    pc.record_pulse(1, 1.0);
    pc.record_confirm(1, true);
    CHECK(pc.verdict(1) == PortVerdict::no_motion);
    pc.record_confirm(2, true);  // kein Puls
    CHECK(pc.verdict(2) == PortVerdict::not_pulsed);
}

TEST_CASE("PortCheck: offene Schritte zaehlen nicht als Problem") {
    PortCheck pc = all_found();
    pc.record_pulse(1, 30.0);
    CHECK(pc.problem_count() == 0);
    CHECK(pc.pending_count() == 2);  // Port 1 unbestaetigt, Port 2 ohne Puls
}

TEST_CASE("PortCheck: Eintrag ohne Bewegungstext wird nicht gepulst") {
    const PortExpect e[] = {{3, DeviceKind::motor, "Intake", nullptr}};
    PortCheck pc(e, 1);
    pc.record_found(3, DeviceKind::motor);
    CHECK(pc.verdict(3) == PortVerdict::ok);
}

TEST_CASE("PortCheck: ungueltige Ports werden ignoriert") {
    PortCheck pc = all_found();
    pc.record_found(0, DeviceKind::motor);
    pc.record_found(22, DeviceKind::motor);
    pc.record_pulse(-1, 10.0);
    pc.record_confirm(99, true);
    CHECK(pc.problem_count() == 0);
    CHECK(pc.verdict(0) == PortVerdict::ok);
}

TEST_CASE("PortCheck: nicht gescannter Port mit Erwartung ist 'missing'") {
    PortCheck pc(kExpect, 3);
    CHECK(pc.verdict(1) == PortVerdict::missing);
    CHECK(pc.problem_count() == 3);
}

TEST_CASE("PortCheck: Problemzeilen") {
    PortCheck pc = all_found();
    pc.record_found(2, DeviceKind::none);
    pc.record_pulse(1, 30.0);
    pc.record_confirm(1, false);
    char buf[40];
    REQUIRE(pc.problem_count() == 2);
    pc.format_problem(0, buf, sizeof buf);
    CHECK(std::string(buf) == "P1 Antrieb L: Richtung falsch");
    pc.format_problem(1, buf, sizeof buf);
    CHECK(std::string(buf) == "P2 Antrieb R: fehlt");
    CHECK(pc.format_problem(2, buf, sizeof buf) == 0);
}

TEST_CASE("PortCheck: Zeile passt immer in den Puffer") {
    PortCheck pc(kExpect, 3);
    char buf[8];
    pc.format_problem(0, buf, sizeof buf);
    CHECK(std::strlen(buf) < sizeof buf);
}
