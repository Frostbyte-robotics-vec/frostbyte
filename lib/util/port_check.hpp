#pragma once

#include <cmath>
#include <cstddef>
#include <cstdio>

// port_check (F-03): wertet aus, was der Scan der 21 V5-Ports gefunden hat, gegen die Erwartung aus config/ (Port, Geraeteart,
// Drehrichtung). Reine Logik ohne PROS und ohne dynamischen Speicher; der Scan selbst und die Motorpulse stehen in
// src/port_check.hpp. Ohne Eintrag in der Erwartung gilt ein Port als leer; ein dort gefundenes Geraet ist "unexpected".
namespace frostbyte {

enum class DeviceKind : unsigned char { none, motor, rotation, imu, gps, other };

struct PortExpect {
    int port;  // 1..21
    DeviceKind kind;
    const char* name;  // kurz, fuer die LCD-Zeile
    int direction;     // Motor: +1 / -1 = erwartete Drehung bei positiver Spannung, 0 = nicht pruefen
};

enum class PulseResult : unsigned char { forward, backward, none };

// Bewegung unter min_deg (oder nicht endlich) gilt als keine Bewegung.
inline PulseResult classify_pulse(double delta_deg, double min_deg) {
    if (!std::isfinite(delta_deg) || std::fabs(delta_deg) < min_deg)
        return PulseResult::none;
    return delta_deg > 0.0 ? PulseResult::forward : PulseResult::backward;
}

enum class PortVerdict : unsigned char { ok, missing, wrong_type, unexpected, not_pulsed, no_motion, wrong_direction };

class PortCheck {
public:
    static constexpr int port_count = 21;
    // Mindestbewegung eines Pulses in Grad (am Motorausgang), darunter gilt der Motor als nicht drehend.
    static constexpr double min_pulse_deg = 5.0;

    PortCheck(const PortExpect* expect, int count) : expect_(expect), count_(count) {}

    void record_found(int port, DeviceKind kind) {
        if (valid(port)) {
            found_[port - 1] = kind;
        }
    }
    void record_pulse(int port, double delta_deg) {
        if (valid(port)) {
            pulse_[port - 1] = classify_pulse(delta_deg, min_pulse_deg);
            pulsed_[port - 1] = true;
        }
    }

    // Ein Port ohne Erwartung und ohne Fund ist ok. Ein Port mit Erwartung, der nie gescannt wurde, gilt als fehlend.
    PortVerdict verdict(int port) const {
        if (!valid(port))
            return PortVerdict::ok;
        const PortExpect* e = find(port);
        const DeviceKind found = found_[port - 1];
        if (!e)
            return found == DeviceKind::none ? PortVerdict::ok : PortVerdict::unexpected;
        if (found == DeviceKind::none)
            return PortVerdict::missing;
        if (found != e->kind)
            return PortVerdict::wrong_type;
        if (e->kind != DeviceKind::motor || e->direction == 0)
            return PortVerdict::ok;
        if (!pulsed_[port - 1])
            return PortVerdict::not_pulsed;
        const PulseResult r = pulse_[port - 1];
        if (r == PulseResult::none)
            return PortVerdict::no_motion;
        const bool forward = r == PulseResult::forward;
        return forward == (e->direction > 0) ? PortVerdict::ok : PortVerdict::wrong_direction;
    }

    // Probleme: alles ausser ok und not_pulsed (der Puls steht nur noch aus).
    int problem_count() const {
        int n = 0;
        for (int p = 1; p <= port_count; ++p)
            n += is_problem(verdict(p)) ? 1 : 0;
        return n;
    }
    int pending_count() const {
        int n = 0;
        for (int p = 1; p <= port_count; ++p)
            n += verdict(p) == PortVerdict::not_pulsed ? 1 : 0;
        return n;
    }

    // Schreibt die index-te Problemzeile (0-basiert) nach buf und gibt deren Laenge zurueck; 0 und leerer Text, wenn es keine gibt.
    std::size_t format_problem(int index, char* buf, std::size_t size) const {
        if (size == 0)
            return 0;
        buf[0] = '\0';
        int seen = 0;
        for (int p = 1; p <= port_count; ++p) {
            const PortVerdict v = verdict(p);
            if (!is_problem(v) || seen++ != index)
                continue;
            const PortExpect* e = find(p);
            const int n = std::snprintf(buf, size, "P%d %s: %s", p, e ? e->name : "?", text(v));
            return n < 0 ? 0 : (static_cast<std::size_t>(n) < size ? static_cast<std::size_t>(n) : size - 1);
        }
        return 0;
    }

    static const char* text(PortVerdict v) {
        switch (v) {
            case PortVerdict::ok: return "ok";
            case PortVerdict::missing: return "fehlt";
            case PortVerdict::wrong_type: return "falsche Art";
            case PortVerdict::unexpected: return "unerwartet";
            case PortVerdict::not_pulsed: return "Puls offen";
            case PortVerdict::no_motion: return "dreht nicht";
            case PortVerdict::wrong_direction: return "Richtung falsch";
        }
        return "?";
    }

private:
    static bool valid(int port) { return port >= 1 && port <= port_count; }
    static bool is_problem(PortVerdict v) { return v != PortVerdict::ok && v != PortVerdict::not_pulsed; }
    const PortExpect* find(int port) const {
        for (int i = 0; i < count_; ++i)
            if (expect_[i].port == port)
                return &expect_[i];
        return nullptr;
    }

    const PortExpect* expect_;
    int count_;
    DeviceKind found_[port_count] = {};
    PulseResult pulse_[port_count] = {};
    bool pulsed_[port_count] = {};
};

}  // namespace frostbyte
