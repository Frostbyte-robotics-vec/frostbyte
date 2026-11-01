#pragma once

#include <algorithm>
#include <cmath>

#include "core/state_machine.hpp"

// Schicht 3: Antrieb (D-01). Arcade-Fahren mit Fahrkurve (Totzone, Exponent), Höchstgeschwindigkeit, Langsam-Modus und
// HOLD. Nur request(), state(), update() wie jedes Subsystem; update() liefert die Ausgabe (Spannung links/rechts,
// Bremse), die die aufrufende Schicht an hal::Motor weitergibt (ADR-011). Jeder Befehl hat ein Timeout: ohne neuen Befehl
// steht der Antrieb (Zustand idle). Eingaben -1..1, Spannung in Volt, Zeit in Sekunden.
// Vorzeichen: positive Drehung (turn > 0) dreht gegen den Uhrzeigersinn (Blickrichtung wächst), die rechte Seite fährt schneller.
namespace frostbyte {

struct DriveConfig {
    double deadband = 0.05;     // Eingaben darunter zählen als 0
    double exponent = 2.0;      // 1 = linear, größer = feinfühliger um die Mitte
    double max_speed = 1.0;     // Höchstgeschwindigkeit pro Fahrer (Anteil der Maximalspannung)
    double slow_factor = 0.4;   // Faktor im Langsam-Modus
    double max_volts = 12.0;
    double timeout_s = 0.25;       // Arcade ohne neuen Befehl
    double hold_timeout_s = 1.0;   // HOLD ohne neuen Befehl
};

// Totzone abziehen, auf 0..1 dehnen, potenzieren, Vorzeichen zurück; Eingabe wird auf -1..1 begrenzt.
inline double shape_input(double x, double deadband, double exponent) {
    const double mag = std::min(std::fabs(x), 1.0);
    if (mag <= deadband)
        return 0.0;
    const double stretched = (mag - deadband) / (1.0 - deadband);
    const double shaped = std::pow(stretched, exponent);
    return x < 0.0 ? -shaped : shaped;
}

enum class DriveMode { stop, arcade, hold };

struct DriveRequest {
    DriveMode mode = DriveMode::stop;
    double forward = 0.0;  // -1..1, vor positiv
    double turn = 0.0;     // -1..1, gegen den Uhrzeigersinn positiv
    bool slow = false;
};

enum class DriveState { idle, driving, holding };

struct DriveOutput {
    double left_v = 0.0;
    double right_v = 0.0;
    bool hold = false;  // true: Motoren bremsen (Hold), sonst frei laufen lassen
};

class DriveSubsystem {
public:
    DriveSubsystem(const DriveConfig& cfg, double now_s) : cfg_(cfg), sm_(DriveState::idle, now_s) {}

    // Ersetzt den laufenden Befehl sofort (manuelle Eingabe übernimmt) und startet sein Timeout neu.
    void request(const DriveRequest& r, double now_s) {
        req_ = r;
        switch (r.mode) {
            case DriveMode::arcade:
                sm_.enter(DriveState::driving, now_s, cfg_.timeout_s, DriveState::idle);
                break;
            case DriveMode::hold:
                sm_.enter(DriveState::holding, now_s, cfg_.hold_timeout_s, DriveState::idle);
                break;
            case DriveMode::stop:
                sm_.enter(DriveState::idle, now_s);
                break;
        }
    }

    DriveState state() const { return sm_.state(); }
    DriveOutput update(double now_s) {
        sm_.update(now_s);
        out_ = DriveOutput{};
        switch (sm_.state()) {
            case DriveState::idle:
                break;
            case DriveState::holding:
                out_.hold = true;
                break;
            case DriveState::driving:
                mix(req_);
                break;
        }
        return out_;
    }

private:
    void mix(const DriveRequest& r) {
        double f = shape_input(r.forward, cfg_.deadband, cfg_.exponent);
        double t = shape_input(r.turn, cfg_.deadband, cfg_.exponent);
        double l = f - t;
        double rt = f + t;
        const double peak = std::max(std::fabs(l), std::fabs(rt));
        if (peak > 1.0) {
            l /= peak;
            rt /= peak;
        }
        const double gain = cfg_.max_volts * cfg_.max_speed * (r.slow ? cfg_.slow_factor : 1.0);
        out_.left_v = l * gain;
        out_.right_v = rt * gain;
    }

    DriveConfig cfg_;
    StateMachine<DriveState> sm_;
    DriveRequest req_;
    DriveOutput out_;
};

}  // namespace frostbyte
