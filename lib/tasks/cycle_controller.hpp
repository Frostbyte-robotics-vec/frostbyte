#pragma once

#include <cmath>
#include <limits>
#include <string_view>

#include "core/state_machine.hpp"
#include "util/goal_book.hpp"
#include "util/rule_guard.hpp"

// Schicht 2: CycleController (C-01, ADR-028). Ein Zyklus = Einheit aufnehmen (Quiver) bzw. schon halten (Preload), greifen,
// heben, zum Goal fahren, setzen, zurückziehen. Reine Logik: pro Takt kommen Meldungen der Schichten darunter hinein
// (CycleInputs), heraus kommt, was der Antrieb, der Intake, der Greifer und der Lift tun sollen (CycleCommand). Die Subsysteme
// rufen sich nicht gegenseitig auf; die aufrufende Schicht verdrahtet sie. Zoll, Sekunden. Zustandsnamen wie docs/log_format.md.
namespace frostbyte {

enum class CycleState { ready, picking, grasping, lifting, at_goal, placing, retreating, error };

// Texte der Spalte `cycle` im Log (docs/log_format.md); kein neuer Name ohne neue Log-Version.
inline const char* cycle_state_name(CycleState s) {
    switch (s) {
        case CycleState::ready: return "BEREIT";
        case CycleState::picking: return "AUFNEHMEN";
        case CycleState::grasping: return "GREIFEN";
        case CycleState::lifting: return "HEBEN";
        case CycleState::at_goal: return "AM_GOAL";
        case CycleState::placing: return "SETZEN";
        case CycleState::retreating: return "RUECKZUG";
        case CycleState::error: return "FEHLER";
    }
    return "FEHLER";
}

// Spiegel von RouteSource (match/route.hpp); der RoutePlayer bildet ab (Schichten nur nach unten).
enum class CycleSource { preload, quiver };

struct CycleRequest {
    std::string_view goal;  // Name aus dem GoalBook
    int level = 0;          // 0..5; `auto` löst der RoutePlayer vorher auf
    CycleSource source = CycleSource::preload;
    double timeout_s = -1.0;  // ganzer Zyklus; <= 0: nur die Zustands-Timeouts
};

struct CycleConfig {
    // Zustands-Timeouts: Startwerte, am Roboter zu prüfen.
    double pick_timeout_s = 4.0;
    double grasp_timeout_s = 1.5;
    double lift_timeout_s = 3.0;
    double approach_timeout_s = 5.0;
    double place_timeout_s = 1.5;
    double retreat_timeout_s = 2.0;
    double retreat_lift_in = std::numeric_limits<double>::quiet_NaN();  // Lifthöhe beim Rückzug; NaN: halten
    // Höhe des Greifers über der gemessenen Lifthöhe. TODO(HW) je Roboter in config/; NaN = nicht gemessen: Zyklen werden
    // abgelehnt (geraten wird nichts).
    double grip_offset_in = std::numeric_limits<double>::quiet_NaN();
};

enum class CycleRefusal {
    none,
    busy,
    bad_config,
    goal_unknown,
    level_invalid,
    level_unknown,
    offset_unknown,
    too_high,
    cannot_grab
};

// Meldungen der Schichten darunter; jede gilt nur für den Takt, in dem sie wahr ist (Pegel, nicht Flanke).
struct CycleInputs {
    bool unit_present = false;  // Intake/Bumper: eine Einheit ist aufgenommen
    bool claw_closed = false;   // Greifer fest (Plateau erkannt)
    bool claw_open = false;     // Greifer offen, Objekt losgelassen
    bool lift_at_target = false;
    bool at_goal = false;       // Antrieb steht an der Setzpose
    bool retreat_done = false;  // Antrieb hat den Rückzug beendet
};

enum class CycleDrive { stop, to_source, to_goal, retreat };
enum class CycleClaw { hold, close, open };  // hold: kein neuer Befehl, Greifer bleibt wie er ist

struct CycleCommand {
    CycleDrive drive = CycleDrive::stop;
    bool intake_on = false;
    CycleClaw claw = CycleClaw::hold;
    double lift_target_in = std::numeric_limits<double>::quiet_NaN();  // NaN: aktuelle Höhe halten
};

// Gilt vom Ende des Zyklus bis zum nächsten Befehl.
struct CycleResult {
    bool finished = false;  // Zyklus ist beendet (fertig, Timeout oder abgebrochen)
    bool placed = false;    // das Objekt wurde gesetzt (SETZEN abgeschlossen)
    bool cancelled = false;
    bool timed_out = false;
    CycleState timeout_state = CycleState::ready;  // Zustand, in dem das Timeout auslöste (nur mit timed_out)
    // Gelungen: gesetzt und nicht abgebrochen. Ein Timeout im RUECKZUG ändert daran nichts: das Objekt liegt.
    bool ok() const { return placed && !cancelled; }
};

class CycleController {
public:
    CycleController(const CycleConfig& cfg, const GoalBook& book, const RuleGuard& guard, double now_s)
        : cfg_(cfg), book_(book), guard_(guard), sm_(CycleState::ready, now_s) {}

    // Startet einen Zyklus aus BEREIT oder FEHLER. pins/cups: aktueller Besitz (< 0 = unbekannt). Bei einer Ablehnung
    // ändert sich nichts. Preload: der Besitz enthält die Einheit schon, `may_grab` wird nicht gefragt.
    CycleRefusal request_cycle(const CycleRequest& req, int pins, int cups, double now_s) {
        if (running(sm_.state()))
            return CycleRefusal::busy;
        if (!timeouts_valid() || !book_.valid())
            return CycleRefusal::bad_config;
        const int goal = book_.index_of(req.goal);
        if (goal < 0)
            return CycleRefusal::goal_unknown;
        if (req.level < 0 || req.level > GoalBook::kLevels - 1)
            return CycleRefusal::level_invalid;
        const auto height = book_.place_height_in(goal, req.level);
        if (!height)
            return CycleRefusal::level_unknown;
        if (!std::isfinite(cfg_.grip_offset_in))
            return CycleRefusal::offset_unknown;
        const double target_in = *height + cfg_.grip_offset_in;
        if (!(target_in <= guard_.soft_lift_ceiling_in()))
            return CycleRefusal::too_high;
        if (req.source == CycleSource::quiver) {
            const GuardInput g{.pins = pins, .cups = cups};
            if (!guard_.may_grab(Grab::pin, g) && !guard_.may_grab(Grab::cup, g))
                return CycleRefusal::cannot_grab;
        }
        target_in_ = target_in;
        start_s_ = now_s;
        cycle_timeout_s_ = req.timeout_s > 0.0 ? req.timeout_s : std::numeric_limits<double>::infinity();
        result_ = CycleResult{};
        enter(req.source == CycleSource::quiver ? CycleState::picking : CycleState::grasping, now_s);
        return CycleRefusal::none;
    }

    // Bricht ab: zurück nach BEREIT (Antrieb steht, Lift hält, Greifer bleibt). Aus FEHLER nur der Zustandswechsel, das
    // Ergebnis bleibt erhalten. In BEREIT ohne Wirkung.
    void cancel(double now_s) {
        if (running(sm_.state())) {
            result_.finished = true;
            result_.cancelled = true;
            sm_.enter(CycleState::ready, now_s);
        } else if (sm_.state() == CycleState::error) {
            sm_.enter(CycleState::ready, now_s);
        }
    }

    CycleState state() const { return sm_.state(); }
    const CycleResult& result() const { return result_; }

    CycleCommand update(double now_s, const CycleInputs& in) {
        const CycleState s = sm_.state();
        if (running(s)) {
            if (triggered(s, in)) {
                advance(s, now_s);
            } else if (now_s - start_s_ >= cycle_timeout_s_) {
                timeout(s, now_s);
            } else if (sm_.update(now_s)) {
                timeout(s, now_s);
            }
        }
        return command(sm_.state());
    }

private:
    static bool running(CycleState s) { return s != CycleState::ready && s != CycleState::error; }

    bool timeouts_valid() const {
        for (double t : {cfg_.pick_timeout_s, cfg_.grasp_timeout_s, cfg_.lift_timeout_s, cfg_.approach_timeout_s,
                         cfg_.place_timeout_s, cfg_.retreat_timeout_s})
            if (!(std::isfinite(t) && t > 0.0))
                return false;
        return true;
    }

    static bool triggered(CycleState s, const CycleInputs& in) {
        switch (s) {
            case CycleState::picking: return in.unit_present;
            case CycleState::grasping: return in.claw_closed;
            case CycleState::lifting: return in.lift_at_target;
            case CycleState::at_goal: return in.at_goal;
            case CycleState::placing: return in.claw_open;
            case CycleState::retreating: return in.retreat_done;
            case CycleState::ready:
            case CycleState::error: break;
        }
        return false;
    }

    // Betritt einen laufenden Zustand mit seinem Timeout; bei Timeout geht es nach FEHLER (timeout() korrigiert RUECKZUG).
    void enter(CycleState s, double now_s) {
        switch (s) {
            case CycleState::picking: sm_.enter(s, now_s, cfg_.pick_timeout_s, CycleState::error); break;
            case CycleState::grasping: sm_.enter(s, now_s, cfg_.grasp_timeout_s, CycleState::error); break;
            case CycleState::lifting: sm_.enter(s, now_s, cfg_.lift_timeout_s, CycleState::error); break;
            case CycleState::at_goal: sm_.enter(s, now_s, cfg_.approach_timeout_s, CycleState::error); break;
            case CycleState::placing: sm_.enter(s, now_s, cfg_.place_timeout_s, CycleState::error); break;
            case CycleState::retreating: sm_.enter(s, now_s, cfg_.retreat_timeout_s, CycleState::ready); break;
            case CycleState::ready:
            case CycleState::error: sm_.enter(s, now_s); break;
        }
    }

    void advance(CycleState from, double now_s) {
        switch (from) {
            case CycleState::picking: enter(CycleState::grasping, now_s); break;
            case CycleState::grasping: enter(CycleState::lifting, now_s); break;
            case CycleState::lifting: enter(CycleState::at_goal, now_s); break;
            case CycleState::at_goal: enter(CycleState::placing, now_s); break;
            case CycleState::placing:
                result_.placed = true;
                enter(CycleState::retreating, now_s);
                break;
            case CycleState::retreating:
                result_.finished = true;
                enter(CycleState::ready, now_s);
                break;
            case CycleState::ready:
            case CycleState::error: break;
        }
    }

    // Timeout in `from`. Nach dem Setzen (RUECKZUG) liegt das Objekt schon: der Zyklus endet in BEREIT, kein Fehlschlag.
    void timeout(CycleState from, double now_s) {
        result_.finished = true;
        result_.timed_out = true;
        result_.timeout_state = from;
        enter(from == CycleState::retreating ? CycleState::ready : CycleState::error, now_s);
    }

    CycleCommand command(CycleState s) const {
        CycleCommand c;
        switch (s) {
            case CycleState::picking:
                c.drive = CycleDrive::to_source;
                c.intake_on = true;
                break;
            case CycleState::grasping: c.claw = CycleClaw::close; break;
            case CycleState::lifting:
                c.claw = CycleClaw::close;
                c.lift_target_in = target_in_;
                break;
            case CycleState::at_goal:
                c.drive = CycleDrive::to_goal;
                c.claw = CycleClaw::close;
                c.lift_target_in = target_in_;
                break;
            case CycleState::placing:
                c.claw = CycleClaw::open;
                c.lift_target_in = target_in_;
                break;
            case CycleState::retreating:
                c.drive = CycleDrive::retreat;
                c.lift_target_in = cfg_.retreat_lift_in;
                break;
            case CycleState::ready:
            case CycleState::error: break;  // passiv: Antrieb steht, Lift hält, Greifer bleibt
        }
        return c;
    }

    CycleConfig cfg_;
    const GoalBook& book_;
    const RuleGuard& guard_;
    StateMachine<CycleState> sm_;
    CycleResult result_;
    double target_in_ = std::numeric_limits<double>::quiet_NaN();
    double start_s_ = 0.0;
    double cycle_timeout_s_ = std::numeric_limits<double>::infinity();
};

}  // namespace frostbyte
