#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

#include "core/state_machine.hpp"

// Schicht 3: Claw (M-03, ADR-038). Öffnen bis zum Anschlag (nullt die Lage), Schließen mit Erkennung des Strom-Plateaus:
// Die Backe steht (Geschwindigkeit unter Schwelle) und der Strom liegt über der Schwelle des Modus, beides plateau_s in Folge
// und erst nach der Anlaufzeit (Anlaufstrom zählt nicht). Steht die Backe schon fast am leeren Ende des Wegs, wurde nichts
// gegriffen. Modus Cup/Pin bestimmt Schließ-/Haltespannung und Stromschwelle. Nur request_*(), state(), update() wie jedes
// Subsystem. Volt, Grad, Ampere, Sekunden. Der Strom kommt als Eingang (ClawSensor); die HAL liefert ihn später.
namespace frostbyte {

enum class ClawMode { cup, pin };

struct ClawModeConfig {
    double close_v = 0.0;  // Schließspannung (positiv)
    double hold_v = 0.0;   // Haltespannung nach dem Griff (>= 0), klein genug gegen Überhitzen
    double plateau_a =
        std::numeric_limits<double>::quiet_NaN();  // Strom, ab dem der Stillstand als Griff zählt (messen; NaN sperrt)
};

struct ClawConfig {
    double open_v = 0.0;  // Öffnen (negativ); Anschlag = Stillstand trotz Spannung
    double empty_travel_deg =
        std::numeric_limits<double>::quiet_NaN();  // Weg offen -> ganz zu ohne Objekt (messen; NaN sperrt)
    double empty_margin_deg = 5.0;                 // näher als das am leeren Ende gilt als nichts gegriffen
    double grace_s = 0.2;                          // Anlaufzeit nach jedem Start, in der Stillstand/Strom nicht zählt
    double stall_deg_s = 10.0;                     // langsamer als das gilt als "steht"
    double plateau_s = 0.1;                        // so lange in Folge steht + Strom über Schwelle -> Griff
    double open_settle_s = 0.1;                    // so lange in Folge am Anschlag -> offen
    double open_timeout_s = 1.5;
    double close_timeout_s = 1.5;
    ClawModeConfig cup;
    ClawModeConfig pin;
};

enum class ClawState { unknown, opening, open, closing, held, error };
enum class ClawError { none, config, sensor, open_timeout };
enum class ClawResult { none, gripped, missed };

struct ClawSensor {
    double position_deg = 0.0;  // Rohwinkel der Backe, aufsummiert
    double velocity_deg_s = 0.0;
    double current_a = 0.0;
    bool ok = true;  // false: Motor nicht erkannt (HAL)
};

struct ClawOutput {
    double volts = 0.0;
};

class ClawSubsystem {
public:
    ClawSubsystem(const ClawConfig& cfg, double now_s) : cfg_(cfg), sm_(ClawState::unknown, now_s) {
        const bool ok = std::isfinite(cfg.open_v) && cfg.open_v < 0.0 && std::isfinite(cfg.empty_travel_deg) &&
                        std::isfinite(cfg.empty_margin_deg) && cfg.empty_margin_deg >= 0.0 &&
                        cfg.empty_travel_deg > cfg.empty_margin_deg && std::isfinite(cfg.grace_s) &&
                        cfg.grace_s >= 0.0 && std::isfinite(cfg.stall_deg_s) && cfg.stall_deg_s >= 0.0 &&
                        std::isfinite(cfg.plateau_s) && cfg.plateau_s > 0.0 && std::isfinite(cfg.open_settle_s) &&
                        cfg.open_settle_s > 0.0 && std::isfinite(cfg.open_timeout_s) && cfg.open_timeout_s > 0.0 &&
                        std::isfinite(cfg.close_timeout_s) && cfg.close_timeout_s > 0.0 && mode_ok(cfg.cup) &&
                        mode_ok(cfg.pin);
        if (!ok) {
            error_ = ClawError::config;
            sm_.enter(ClawState::error, now_s);
        }
    }

    // Öffnet bis zum Anschlag (auch aus held, closing, error); die Lage ist danach neu genullt.
    bool request_open(double now_s) {
        if (error_ == ClawError::config)
            return false;
        error_ = ClawError::none;
        begin_opening(now_s);
        return true;
    }

    // Schließt im Modus; nur aus open (die Nulllage muss bekannt sein). Löscht das letzte Ergebnis.
    bool request_close(ClawMode mode, double now_s) {
        if (sm_.state() != ClawState::open || error_ != ClawError::none)
            return false;
        mode_ = mode;
        result_ = ClawResult::none;
        still_s_ = 0.0;
        sm_.enter(ClawState::closing, now_s, cfg_.close_timeout_s, ClawState::closing);
        return true;
    }

    ClawState state() const { return sm_.state(); }
    ClawError error() const { return error_; }
    ClawResult result() const { return result_; }  // gripped / missed des letzten Schließens
    double travel_deg() const { return travel_deg_; }

    ClawOutput update(double now_s, const ClawSensor& s) {
        const double dt_s = have_last_ ? std::max(0.0, now_s - last_s_) : 0.0;
        have_last_ = true;
        last_s_ = now_s;
        if (error_ == ClawError::config)
            return ClawOutput{};
        if (!s.ok || !std::isfinite(s.position_deg) || !std::isfinite(s.velocity_deg_s) ||
            !std::isfinite(s.current_a)) {
            // Ohne Sensor ist die Lage unbekannt: 0 V, erst ein neues Öffnen heilt.
            error_ = ClawError::sensor;
            sm_.enter(ClawState::error, now_s);
            return ClawOutput{};
        }
        sm_.update(now_s);
        switch (sm_.state()) {
            case ClawState::unknown:
            case ClawState::open: return ClawOutput{};
            case ClawState::error:
                if (error_ == ClawError::none)
                    error_ = ClawError::open_timeout;  // Timeout des Öffnens
                return ClawOutput{};
            case ClawState::opening: return opening(now_s, s, dt_s);
            case ClawState::closing: return closing(now_s, s, dt_s);
            case ClawState::held: return ClawOutput{clamp_v(mode_cfg().hold_v)};
        }
        return ClawOutput{};
    }

private:
    static bool mode_ok(const ClawModeConfig& m) {
        return std::isfinite(m.close_v) && m.close_v > 0.0 && std::isfinite(m.hold_v) && m.hold_v >= 0.0 &&
               std::isfinite(m.plateau_a) && m.plateau_a > 0.0;
    }
    static double clamp_v(double v) { return std::clamp(v, -12.0, 12.0); }
    const ClawModeConfig& mode_cfg() const { return mode_ == ClawMode::cup ? cfg_.cup : cfg_.pin; }

    void begin_opening(double now_s) {
        still_s_ = 0.0;
        sm_.enter(ClawState::opening, now_s, cfg_.open_timeout_s, ClawState::error);
    }

    ClawOutput opening(double now_s, const ClawSensor& s, double dt_s) {
        if (sm_.elapsed_s(now_s) >= cfg_.grace_s) {
            still_s_ = std::fabs(s.velocity_deg_s) < cfg_.stall_deg_s ? still_s_ + dt_s : 0.0;
            if (still_s_ >= cfg_.open_settle_s) {
                open_pos_deg_ = s.position_deg;
                sm_.enter(ClawState::open, now_s);
                return ClawOutput{};
            }
        }
        return ClawOutput{clamp_v(cfg_.open_v)};
    }

    ClawOutput closing(double now_s, const ClawSensor& s, double dt_s) {
        const ClawModeConfig& m = mode_cfg();
        travel_deg_ = s.position_deg - open_pos_deg_;
        if (sm_.timed_out() || travel_deg_ >= cfg_.empty_travel_deg - cfg_.empty_margin_deg) {
            result_ = ClawResult::missed;  // Timeout oder am leeren Ende angekommen: nichts gegriffen
            begin_opening(now_s);
            return ClawOutput{clamp_v(cfg_.open_v)};
        }
        if (sm_.elapsed_s(now_s) >= cfg_.grace_s) {
            const bool plateau = std::fabs(s.velocity_deg_s) < cfg_.stall_deg_s && s.current_a >= m.plateau_a;
            still_s_ = plateau ? still_s_ + dt_s : 0.0;
            if (still_s_ >= cfg_.plateau_s) {
                result_ = ClawResult::gripped;
                sm_.enter(ClawState::held, now_s);
                return ClawOutput{clamp_v(m.hold_v)};
            }
        }
        return ClawOutput{clamp_v(m.close_v)};
    }

    ClawConfig cfg_;
    StateMachine<ClawState> sm_;
    ClawError error_ = ClawError::none;
    ClawResult result_ = ClawResult::none;
    ClawMode mode_ = ClawMode::cup;
    bool have_last_ = false;
    double last_s_ = 0.0, still_s_ = 0.0, open_pos_deg_ = 0.0, travel_deg_ = 0.0;
};

}  // namespace frostbyte
