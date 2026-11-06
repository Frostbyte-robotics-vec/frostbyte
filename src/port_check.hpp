#pragma once

#include <cstdint>
#include <cstdio>

#include "main.h"
#include "util/port_check.hpp"

// port_check (F-03): Hilfsprogramm fuer den aufgebockten Roboter, aktiv nur mit config::port_check_enabled (dann startet
// initialize() keinen Steuer-Task). Ablauf: alle 21 Ports scannen, Ergebnis aufs LCD und ins Terminal; auf Taste A am
// Controller jeden Motor mit Bewegungstext kurz mit +4 V pulsen (Port mit Vorzeichen wie in config/). Der Encoder zeigt nur, ob
// sich der Motor dreht; ob sich die Mechanik in die erwartete Richtung bewegt, bestaetigt der Mensch am Controller (A = richtig,
// B = falsch). "Richtung falsch" heisst: Vorzeichen des Ports in config/ umdrehen. Eigene Schleife mit delay(), darum nie zusammen mit dem Steuer-Task.
namespace frostbyte {

inline DeviceKind to_device_kind(pros::DeviceType t) {
    switch (t) {
        case pros::DeviceType::none: return DeviceKind::none;
        case pros::DeviceType::motor: return DeviceKind::motor;
        case pros::DeviceType::rotation: return DeviceKind::rotation;
        case pros::DeviceType::imu: return DeviceKind::imu;
        case pros::DeviceType::gps: return DeviceKind::gps;
        default: return DeviceKind::other;
    }
}

inline void port_check_show(const PortCheck& pc, const char* hint) {
    pros::lcd::print(1, "problems %d  offen %d", pc.problem_count(), pc.pending_count());
    char line[40];
    for (int i = 0; i < 5; ++i) {
        pc.format_problem(i, line, sizeof line);
        pros::lcd::print(2 + i, "%s", line);  // leerer Text loescht eine alte Zeile
    }
    pros::lcd::print(7, "%s", hint);
}

inline void port_check_run(const PortExpect* expect, int count) {
    constexpr double pulse_v = 4.0;
    constexpr std::uint32_t pulse_ms = 300;
    PortCheck pc(expect, count);
    for (int port = 1; port <= PortCheck::port_count; ++port) {
        const DeviceKind kind = to_device_kind(pros::Device::get_plugged_type(static_cast<std::uint8_t>(port)));
        pc.record_found(port, kind);
        if (kind != DeviceKind::none)
            std::printf("port %d: Geraeteart %d\n", port, static_cast<int>(kind));
    }
    if (const int open = unassigned_count(expect, count); open > 0)
        std::printf("%d Port(s) in config/ noch offen (TODO(HW)), werden uebersprungen\n", open);
    port_check_show(pc, "A: Motoren pulsen (aufgebockt)");

    pros::Controller master(pros::E_CONTROLLER_MASTER);
    while (!master.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_A))
        pros::delay(20);

    for (int i = 0; i < count; ++i) {
        const PortExpect& e = expect[i];
        if (!port_valid(
                e.port))  // Port steht noch nicht fest (TODO(HW) in config/): nie auf Port 0 oder einem geratenen pulsen
            continue;
        const int port = e.port < 0 ? -e.port : e.port;
        const PortVerdict v = pc.verdict(port);
        if (e.kind != DeviceKind::motor || !e.motion || v == PortVerdict::missing || v == PortVerdict::wrong_type)
            continue;
        pros::lcd::print(1, "P%d %s", port, e.name);
        pros::lcd::print(2, "gleich +4V, erwartet:");
        pros::lcd::print(3, "%s", e.motion);
        pros::lcd::print(4, "(Lift/Arm abstuetzen)");
        pros::delay(1500);
        pros::Motor m(static_cast<std::int8_t>(e.port));
        m.set_encoder_units(pros::v5::MotorUnits::degrees);
        m.set_brake_mode(pros::v5::MotorBrake::hold);  // Lift/Arm sacken nach dem Puls nicht ab
        const double start = m.get_position();
        m.move_voltage(static_cast<std::int32_t>(pulse_v * 1000.0));
        pros::delay(pulse_ms);
        m.move_voltage(0);
        m.brake();
        pc.record_pulse(port, m.get_position() - start);
        if (pc.verdict(port) == PortVerdict::unconfirmed) {
            pros::lcd::print(5, "A: richtig  B: falsch");
            while (true) {
                if (master.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_A)) {
                    pc.record_confirm(port, true);
                    break;
                }
                if (master.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_B)) {
                    pc.record_confirm(port, false);
                    break;
                }
                pros::delay(20);
            }
        }
        std::printf("port %d (%s): %s\n", port, e.name, PortCheck::text(pc.verdict(port)));
        m.move_voltage(0);  // loest die Haltebremse
        pros::delay(200);
    }
    port_check_show(pc, pc.problem_count() == 0 ? "alles ok" : "Probleme: config/ korrigieren");
    while (true)
        pros::delay(100);
}

}  // namespace frostbyte
