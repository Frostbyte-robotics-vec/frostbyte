#include <cmath>
#include <cstring>
#include <string>

#include "doctest/doctest.h"
#include "util/port_check.hpp"

using namespace frostbyte;

namespace {
const PortExpect kExpect[] = {
    {1, DeviceKind::motor, "Antrieb L", +1},
    {2, DeviceKind::motor, "Antrieb R", -1},
    {5, DeviceKind::imu, "IMU", 0},
};
}

TEST_CASE("classify_pulse: Vorzeichen und Mindestbewegung") {
    CHECK(classify_pulse(12.0, 5.0) == PulseResult::forward);
    CHECK(classify_pulse(-12.0, 5.0) == PulseResult::backward);
    CHECK(classify_pulse(4.9, 5.0) == PulseResult::none);
    CHECK(classify_pulse(-4.9, 5.0) == PulseResult::none);
    CHECK(classify_pulse(std::nan(""), 5.0) == PulseResult::none);
}

TEST_CASE("PortCheck: alles wie erwartet -> keine Probleme") {
    PortCheck pc(kExpect, 3);
    pc.record_found(1, DeviceKind::motor);
    pc.record_found(2, DeviceKind::motor);
    pc.record_found(5, DeviceKind::imu);
    pc.record_pulse(1, +30.0);
    pc.record_pulse(2, -30.0);
    CHECK(pc.verdict(1) == PortVerdict::ok);
    CHECK(pc.verdict(2) == PortVerdict::ok);
    CHECK(pc.verdict(5) == PortVerdict::ok);
    CHECK(pc.problem_count() == 0);
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

TEST_CASE("PortCheck: Drehrichtung") {
    PortCheck pc(kExpect, 3);
    pc.record_found(1, DeviceKind::motor);
    pc.record_found(2, DeviceKind::motor);
    CHECK(pc.verdict(1) == PortVerdict::not_pulsed);  // Motor erkannt, Puls fehlt noch
    pc.record_pulse(1, -30.0);                        // erwartet vorwaerts
    pc.record_pulse(2, 1.0);                          // bewegt sich nicht
    CHECK(pc.verdict(1) == PortVerdict::wrong_direction);
    CHECK(pc.verdict(2) == PortVerdict::no_motion);
}

TEST_CASE("PortCheck: not_pulsed zaehlt nicht als Problem, Richtung 0 wird nicht geprueft") {
    PortCheck pc(kExpect, 3);
    pc.record_found(1, DeviceKind::motor);
    pc.record_found(2, DeviceKind::motor);
    pc.record_found(5, DeviceKind::imu);
    CHECK(pc.problem_count() == 0);
    CHECK(pc.pending_count() == 2);
}

TEST_CASE("PortCheck: ungueltige Ports werden ignoriert") {
    PortCheck pc(kExpect, 3);
    pc.record_found(1, DeviceKind::motor);
    pc.record_found(2, DeviceKind::motor);
    pc.record_found(5, DeviceKind::imu);
    pc.record_found(0, DeviceKind::motor);
    pc.record_found(22, DeviceKind::motor);
    pc.record_pulse(-1, 10.0);
    CHECK(pc.problem_count() == 0);
    CHECK(pc.verdict(0) == PortVerdict::ok);
}

TEST_CASE("PortCheck: nicht gescannter Port mit Erwartung ist 'missing'") {
    PortCheck pc(kExpect, 3);  // nichts aufgezeichnet
    CHECK(pc.verdict(1) == PortVerdict::missing);
    CHECK(pc.problem_count() == 3);
}

TEST_CASE("PortCheck: Problemzeilen") {
    PortCheck pc(kExpect, 3);
    pc.record_found(1, DeviceKind::motor);
    pc.record_found(2, DeviceKind::none);
    pc.record_found(5, DeviceKind::imu);
    pc.record_pulse(1, -30.0);
    char buf[40];
    REQUIRE(pc.problem_count() == 2);
    pc.format_problem(0, buf, sizeof buf);
    CHECK(std::string(buf) == "P1 Antrieb L: Richtung falsch");
    pc.format_problem(1, buf, sizeof buf);
    CHECK(std::string(buf) == "P2 Antrieb R: fehlt");
    CHECK(pc.format_problem(2, buf, sizeof buf) == 0);  // kein weiteres Problem
}

TEST_CASE("PortCheck: Zeile passt immer in den Puffer") {
    PortCheck pc(kExpect, 3);
    char buf[8];
    pc.format_problem(0, buf, sizeof buf);
    CHECK(std::strlen(buf) < sizeof buf);
}
