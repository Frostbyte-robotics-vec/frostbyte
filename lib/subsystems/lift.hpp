#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "core/state_machine.hpp"
#include "util/monotone_table.hpp"

// Schicht 3: Lift (M-01, ADR-019). Homing gegen den unteren Anschlag, Höhe anfahren, Höhe halten, Software-Endlagen und
// eine Decke von außen (Endgame-Höhe, später RuleGuard M-07). Nur request_*(), state(), update() wie jedes Subsystem;
// update() liefert Spannung und Bremse für die aufrufende Schicht. Zoll, Grad, Sekunden, Volt. Die Sensorwinkel sind
// relativ zur Home-Lage; die Kalibriertabelle (5 Punkte, Winkel -> Höhe) enthält die Übersetzung, geraten wird nichts.
namespace frostbyte {

struct LiftConfig {
    std::array<double, 5> cal_angle_deg{};  // Sensorwinkel relativ zu Home, streng steigend
    std::array<double, 5> cal_height_in{};  // gemessene Höhe dazu, streng steigend
    double min_in = 0.0;                    // Software-Endlagen
    double max_in = 0.0;
    double margin_in = 0.5;     // Spiel außerhalb der Endlagen, ab dem die Spannung nach außen gesperrt wird
    double kp_v_per_deg = 0.0;  // Lageregler
    double kg_v = 0.0;          // Haltespannung gegen die Schwerkraft (positiv = nach oben)
    double max_up_v = 12.0;     // Spannungsgrenzen je Richtung
    double max_down_v = 6.0;
    double tol_in = 0.3;         // angekommen innerhalb dieser Höhenabweichung ...
    double tol_vel_deg_s = 5.0;  // ... und bei kleinerer Geschwindigkeit
    double move_timeout_s = 3.0;
    double home_v = -3.0;           // Spannung beim Homing (Vorzeichen: Richtung zum unteren Anschlag)
    double home_grace_s = 0.3;      // Anlaufzeit, in der Stillstand noch nicht zählt
    double home_stall_deg_s = 2.0;  // langsamer als das gilt als "steht am Anschlag"
    double home_stall_s = 0.25;     // so lange in Folge
    double home_timeout_s = 5.0;
};

enum class LiftState { unhomed, homing, moving, holding, error };
enum class LiftError { none, config, sensor, home_timeout };

struct LiftSensor {
    double position_deg = 0.0;  // Rohwinkel, aufsummiert
    double velocity_deg_s = 0.0;
    bool ok = true;  // false: Sensor nicht erkannt (HAL)
};

struct LiftOutput {
    double volts = 0.0;
    bool brake = true;  // true: Motor bremsen statt frei lassen (unhomed, error)
};

class LiftSubsystem {
public:
    LiftSubsystem(const LiftConfig& cfg, double now_s) : cfg_(cfg), sm_(LiftState::unhomed, now_s) {
        const bool ok = table_.set(cfg.cal_angle_deg, cfg.cal_height_in) && std::isfinite(cfg.min_in) &&
                        std::isfinite(cfg.max_in) && cfg.min_in < cfg.max_in && cfg.move_timeout_s > 0.0 &&
                        cfg.home_timeout_s > 0.0;
        if (!ok) {
            error_ = LiftError::config;
            sm_.enter(LiftState::error, now_s);
        }
    }

    // Höhe über dem Home-Winkel und zurück (Kalibriertabelle, begrenzt auf den Tabellenbereich).
    double height_for_angle(double rel_deg) const { return table_.eval(rel_deg); }
    double angle_for_height(double height_in) const { return table_.invert(height_in); }

    // Startet das Homing (auch aus holding/moving/error, wenn der Sensor da ist). Zuerst unhomed: bis zum Anschlag ist die
    // Lage unbekannt.
    bool request_home(double now_s) {
        if (error_ == LiftError::config)
            return false;
        error_ = LiftError::none;
        timed_out_ = false;
        homed_ = false;
        have_last_ = false;
        still_s_ = 0.0;
        sm_.enter(LiftState::homing, now_s, cfg_.home_timeout_s, LiftState::error);
        return true;
    }

    // Fährt zu height_in (auf die Endlagen und die Decke begrenzt). Nur gehomed und ohne Fehler; sonst false und nichts ändert
    // sich. Ein neuer Befehl ersetzt den laufenden und startet das Timeout neu.
    bool request_move(double height_in, double now_s) {
        if (!homed_ || error_ != LiftError::none || !std::isfinite(height_in))
            return false;
        goal_in_ = height_in;
        timed_out_ = false;
        sm_.enter(LiftState::moving, now_s, cfg_.move_timeout_s, LiftState::holding);
        return true;
    }

    // Hält die aktuell gemessene Höhe.
    bool request_hold(double now_s) {
        if (!homed_ || error_ != LiftError::none)
            return false;
        goal_in_ = height_in_;
        timed_out_ = false;
        sm_.enter(LiftState::holding, now_s);
        return true;
    }

    // Obergrenze von außen (Endgame-Höhe). NaN oder kleiner als min: gilt als min_in.
    void set_ceiling_in(double ceiling_in) { ceiling_in_ = std::isfinite(ceiling_in) ? ceiling_in : cfg_.min_in; }

    LiftState state() const { return sm_.state(); }
    LiftError error() const { return error_; }
    bool homed() const { return homed_; }
    bool timed_out() const { return timed_out_; }  // letzter Fahrbefehl lief in sein Timeout; der Lift hält die Höhe
    double height_in() const { return height_in_; }
    double target_in() const { return effective_goal(); }

    LiftOutput update(double now_s, const LiftSensor& s) {
        sm_.update(now_s);
        if (error_ == LiftError::config)
            return LiftOutput{};
        if (!s.ok || !std::isfinite(s.position_deg) || !std::isfinite(s.velocity_deg_s)) {
            // Ohne Sensor ist die Lage unbekannt: bremsen und erst nach neuem Homing wieder fahren.
            error_ = LiftError::sensor;
            homed_ = false;
            sm_.enter(LiftState::error, now_s);
            return LiftOutput{};
        }
        switch (sm_.state()) {
            case LiftState::unhomed: return LiftOutput{};
            case LiftState::error:
                if (error_ == LiftError::none)
                    error_ = LiftError::home_timeout;  // Timeout des Homings
                homed_ = false;
                return LiftOutput{};
            case LiftState::homing: return homing(now_s, s);
            case LiftState::moving:
            case LiftState::holding: return control(now_s, s);
        }
        return LiftOutput{};
    }

private:
    double effective_goal() const {
        const double ceiling = std::max(cfg_.min_in, std::min(cfg_.max_in, ceiling_in_));
        return std::clamp(goal_in_, cfg_.min_in, ceiling);
    }

    LiftOutput homing(double now_s, const LiftSensor& s) {
        const double dt_s = have_last_ ? now_s - last_s_ : 0.0;
        if (!have_last_)
            started_s_ = now_s;
        have_last_ = true;
        last_s_ = now_s;
        if (now_s - started_s_ >= cfg_.home_grace_s) {
            still_s_ = std::fabs(s.velocity_deg_s) < cfg_.home_stall_deg_s ? still_s_ + dt_s : 0.0;
            if (still_s_ >= cfg_.home_stall_s) {
                zero_deg_ = s.position_deg;
                homed_ = true;
                height_in_ = height_for_angle(0.0);
                goal_in_ = height_in_;
                sm_.enter(LiftState::holding, now_s);
                return control(now_s, s);
            }
        }
        return LiftOutput{cfg_.home_v, false};
    }

    LiftOutput control(double now_s, const LiftSensor& s) {
        const double rel_deg = s.position_deg - zero_deg_;
        height_in_ = height_for_angle(rel_deg);
        // Move-Timeout: der Fahrbefehl ist abgelaufen (StateMachine wechselte nach holding); Höhe halten wie gemessen.
        if (sm_.timed_out() && !timed_out_) {
            timed_out_ = true;
            goal_in_ = height_in_;
        }
        double goal = effective_goal();
        // Liegt das Ziel (Halten oder Fahren) über einer gesenkten Decke, fährt der Lift von selbst nach unten.
        if (sm_.state() == LiftState::holding && std::fabs(goal - goal_in_) > 1e-9 && goal < height_in_ - cfg_.tol_in) {
            goal_in_ = goal;
            sm_.enter(LiftState::moving, now_s, cfg_.move_timeout_s, LiftState::holding);
            timed_out_ = false;
        }
        const double err_deg = angle_for_height(goal) - rel_deg;
        double volts = cfg_.kp_v_per_deg * err_deg + cfg_.kg_v;
        volts = std::clamp(volts, -cfg_.max_down_v, cfg_.max_up_v);
        // Endlagen: nach außen keine Spannung.
        if ((height_in_ >= cfg_.max_in + cfg_.margin_in && volts > 0.0) ||
            (height_in_ <= cfg_.min_in - cfg_.margin_in && volts < 0.0))
            volts = 0.0;
        if (sm_.state() == LiftState::moving && std::fabs(goal - height_in_) <= cfg_.tol_in &&
            std::fabs(s.velocity_deg_s) <= cfg_.tol_vel_deg_s) {
            goal_in_ = goal;
            sm_.enter(LiftState::holding, now_s);
        }
        return LiftOutput{volts, false};
    }

    LiftConfig cfg_;
    MonotoneTable<5> table_;
    StateMachine<LiftState> sm_;
    LiftError error_ = LiftError::none;
    bool homed_ = false;
    bool timed_out_ = false;
    bool have_last_ = false;
    double last_s_ = 0.0, started_s_ = 0.0, still_s_ = 0.0;
    double zero_deg_ = 0.0;
    double height_in_ = 0.0;
    double goal_in_ = 0.0;
    double ceiling_in_ = 1e9;
};

}  // namespace frostbyte
