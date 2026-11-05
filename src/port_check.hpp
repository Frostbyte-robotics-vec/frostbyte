#pragma once

#include <cstdint>
#include <cstdio>

#include "main.h"
#include "util/port_check.hpp"

// port_check (F-03): Hilfsprogramm fuer den aufgebockten Roboter, aktiv nur mit config::port_check_enabled (dann startet
// initialize() keinen Steuer-Task). Ablauf: alle 21 Ports scannen, Ergebnis aufs LCD und ins Terminal; auf Taste A am
// Controller jeden Motor mit Richtung != 0 kurz mit +4 V pulsen und die Drehrichtung des Encoders messen. Die Pulse laufen
// ohne Port-Umkehr: "Richtung falsch" heisst, in config/ den Port negativ eintragen (oder die Erwartung korrigieren).
// Eigene Schleife mit delay(), darum nie zusammen mit dem Steuer-Task.
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
    port_check_show(pc, "A: Motoren pulsen (aufgebockt!)");

    pros::Controller master(pros::E_CONTROLLER_MASTER);
    while (!master.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_A))
        pros::delay(20);

    for (int i = 0; i < count; ++i) {
        const PortExpect& e = expect[i];
        if (e.kind != DeviceKind::motor || e.direction == 0 || pc.verdict(e.port) == PortVerdict::missing ||
            pc.verdict(e.port) == PortVerdict::wrong_type)
            continue;
        pros::Motor m(static_cast<std::int8_t>(e.port));
        m.set_encoder_units(pros::v5::MotorUnits::degrees);
        m.set_brake_mode(pros::v5::MotorBrake::coast);
        const double start = m.get_position();
        m.move_voltage(static_cast<std::int32_t>(pulse_v * 1000.0));
        pros::delay(pulse_ms);
        m.move_voltage(0);
        pc.record_pulse(e.port, m.get_position() - start);
        std::printf("port %d (%s): Puls %s\n", e.port, e.name, PortCheck::text(pc.verdict(e.port)));
        pros::delay(200);
    }
    port_check_show(pc, pc.problem_count() == 0 ? "alles ok" : "Probleme: config/ korrigieren");
    while (true)
        pros::delay(100);
}

}  // namespace frostbyte
