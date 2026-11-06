#pragma once

#include <algorithm>
#include <cmath>

#include "core/state_machine.hpp"

// Schicht 3: Intake (M-04, ADR-037). Rollen laufen, bis der Bumper eine Einheit meldet; klemmt etwas (Rollen stehen trotz
// Spannung), gibt es einen kurzen Rückwärtsimpuls und dann einen neuen Versuch, nach max_retries Impulsen einen Fehler.
// Nur request_*(), state(), update() wie jedes Subsystem; update() liefert die Spannung für die aufrufende Schicht.
// Stau wird aus der Rollen-Geschwindigkeit erkannt (kein Strom nötig, hal::Motor bleibt unverändert). Volt, Grad, Sekunden.
namespace frostbyte {

struct IntakeConfig {
    double run_v = 0.0;          // Spannung beim Einziehen (positiv); Richtung der Rollen legt der Port in config/ fest
    double reverse_v = 0.0;      // Spannung beim Rückwärtsimpuls (negativ)
    double run_timeout_s = 0.0;  // Gesamtzeit eines Laufs (mit Impulsen), danach idle + timed_out()
    double grace_s = 0.3;        // Anlaufzeit nach Start und nach jedem Impuls, in der Stillstand noch nicht zählt
    double stall_deg_s = 20.0;   // langsamer als das gilt als "stehen"
    double jam_s = 0.25;         // so lange in Folge stehen -> Stau
    double reverse_s = 0.4;      // Dauer des Rückwärtsimpulses
    int max_retries = 2;         // Impulse je Lauf; der Stau danach ist ein Fehler
    double bumper_debounce_s = 0.05;
};

enum class IntakeState { idle, running, reversing, holding, error };
enum class IntakeError { none, config, sensor, jam };

struct IntakeSensor {
    bool bumper_pressed = false;
    double velocity_deg_s = 0.0;  // Rollen
    bool ok = true;               // false: Motor nicht erkannt (HAL)
};

struct IntakeOutput {
    double volts = 0.0;
};

class IntakeSubsystem {
public:
    IntakeSubsystem(const IntakeConfig& cfg, double now_s) : cfg_(cfg), sm_(IntakeState::idle, now_s) {
        const bool ok = std::isfinite(cfg.run_v) && cfg.run_v > 0.0 && std::isfinite(cfg.reverse_v) &&
                        cfg.reverse_v < 0.0 && std::isfinite(cfg.run_timeout_s) && cfg.run_timeout_s > 0.0 &&
                        std::isfinite(cfg.grace_s) && cfg.grace_s >= 0.0 && std::isfinite(cfg.stall_deg_s) &&
                        cfg.stall_deg_s >= 0.0 && std::isfinite(cfg.jam_s) && cfg.jam_s > 0.0 &&
                        std::isfinite(cfg.reverse_s) && cfg.reverse_s > 0.0 && cfg.max_retries >= 0 &&
                        std::isfinite(cfg.bumper_debounce_s) && cfg.bumper_debounce_s >= 0.0;
        if (!ok) {
            error_ = IntakeError::config;
            sm_.enter(IntakeState::error, now_s);
        }
    }

    // Startet einen Lauf (auch aus holding und error). Mit gedrücktem Bumper wird daraus nach der Entprellzeit holding.
    bool request_run(double now_s) {
        if (error_ == IntakeError::config)
            return false;
        error_ = IntakeError::none;
        timed_out_ = false;
        jam_count_ = 0;
        run_started_s_ = now_s;
        begin_running(now_s);
        return true;
    }

    // Rollen aus. Die Einheit im Bumper bleibt vermerkt, bis der Bumper loslässt.
    void request_stop(double now_s) {
        if (error_ == IntakeError::config)
            return;
        // Stopp quittiert einen Fehler (Stau, Sensor); ein Sensorfehler kommt im nächsten update() wieder
        error_ = IntakeError::none;
        timed_out_ = false;
        sm_.enter(IntakeState::idle, now_s);
    }

    IntakeState state() const { return sm_.state(); }
    IntakeError error() const { return error_; }
    bool unit_present() const { return unit_; }    // Bumper entprellt gedrückt
    bool timed_out() const { return timed_out_; }  // letzter Lauf lief in sein Timeout
    int jam_count() const { return jam_count_; }   // erkannte Staus seit dem letzten request_run

    IntakeOutput update(double now_s, const IntakeSensor& s) {
        const double dt_s = have_last_ ? std::max(0.0, now_s - last_s_) : 0.0;
        have_last_ = true;
        last_s_ = now_s;
        if (error_ == IntakeError::config)
            return IntakeOutput{};
        if (!s.ok || !std::isfinite(s.velocity_deg_s)) {
            error_ = IntakeError::sensor;
            unit_ = false;
            sm_.enter(IntakeState::error, now_s);
            return IntakeOutput{};
        }
        track_bumper(s.bumper_pressed, dt_s);
        switch (sm_.state()) {
            case IntakeState::idle:
            case IntakeState::error: return IntakeOutput{};
            case IntakeState::holding:
                if (!unit_) {
                    sm_.enter(IntakeState::idle, now_s);
                    break;
                }
                return IntakeOutput{};
            case IntakeState::running:
            case IntakeState::reversing: return drive(now_s, s, dt_s);
        }
        return IntakeOutput{};
    }

private:
    void begin_running(double now_s) {
        still_s_ = 0.0;
        sm_.enter(IntakeState::running, now_s);
    }

    // Bumper entprellt: unit_ wird erst nach debounce Sekunden gesetzt bzw. gelöscht.
    void track_bumper(bool pressed, double dt_s) {
        if (pressed == unit_) {
            change_s_ = 0.0;
            return;
        }
        change_s_ += dt_s;
        if (change_s_ >= cfg_.bumper_debounce_s) {
            unit_ = pressed;
            change_s_ = 0.0;
        }
    }

    IntakeOutput drive(double now_s, const IntakeSensor& s, double dt_s) {
        if (unit_) {
            sm_.enter(IntakeState::holding, now_s);
            return IntakeOutput{};
        }
        if (now_s - run_started_s_ >= cfg_.run_timeout_s) {
            timed_out_ = true;
            sm_.enter(IntakeState::idle, now_s);
            return IntakeOutput{};
        }
        if (sm_.state() == IntakeState::reversing) {
            if (sm_.elapsed_s(now_s) >= cfg_.reverse_s)
                begin_running(now_s);  // Anlaufzeit beginnt neu
            else
                return IntakeOutput{std::clamp(cfg_.reverse_v, -12.0, 12.0)};
        }
        if (sm_.elapsed_s(now_s) >= cfg_.grace_s) {
            still_s_ = std::fabs(s.velocity_deg_s) < cfg_.stall_deg_s ? still_s_ + dt_s : 0.0;
            if (still_s_ >= cfg_.jam_s) {
                ++jam_count_;
                still_s_ = 0.0;
                if (jam_count_ > cfg_.max_retries) {
                    error_ = IntakeError::jam;
                    sm_.enter(IntakeState::error, now_s);
                    return IntakeOutput{};
                }
                sm_.enter(IntakeState::reversing, now_s);
                return IntakeOutput{std::clamp(cfg_.reverse_v, -12.0, 12.0)};
            }
        }
        return IntakeOutput{std::clamp(cfg_.run_v, -12.0, 12.0)};
    }

    IntakeConfig cfg_;
    StateMachine<IntakeState> sm_;
    IntakeError error_ = IntakeError::none;
    bool timed_out_ = false;
    bool unit_ = false;
    bool have_last_ = false;
    int jam_count_ = 0;
    double last_s_ = 0.0, run_started_s_ = 0.0, still_s_ = 0.0, change_s_ = 0.0;
};

}  // namespace frostbyte
