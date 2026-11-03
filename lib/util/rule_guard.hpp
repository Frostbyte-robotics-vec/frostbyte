#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

// RuleGuard (M-07, ADR-027): harte Sperren aus dem Game Manual v2.1, ganz unten in lib/ wie GoalBook (ADR-022), weil
// sowohl der CycleController ("darf ich greifen?") als auch die Ausgabestufe des Steuer-Tasks ("letzter Filter vor
// der HAL") nach unten aufrufen. Kein Subsystem (kein request/update/Timeout), keine PROS-Header, kein dynamischer Speicher.
// Zoll, Volt, Sekunden. Unbekannte Eingaben führen immer zur sicheren Seite: nichts greifen, Lift nicht nach oben.
namespace frostbyte {

// Regelkonstanten (Manual, keine Modellzahlen). Grenzen sind einschließlich: "can never be larger than 24"" (SG2a),
// "no part ... may exceed 50"" (SG3), "maximum of one Pin and one Cup" (SG6).
constexpr double kMaxFootprintIn = 24.0;
constexpr double kMaxHeightIn = 50.0;
constexpr int kMaxPins = 1;
constexpr int kMaxCups = 1;

// Verletzte bzw. eingreifende Regel als Bitmaske; in der Simulation zu RULE:SG2 usw. abgebildet.
enum RuleBit : std::uint8_t { kRuleSg2 = 1, kRuleSg3 = 2, kRuleSg6 = 4, kRuleMatchEnd = 8 };

// Zustand gegen die Regeln prüfen (reine Funktion, ohne Eingriff). pins/cups < 0 = unbekannt: keine Meldung.
inline std::uint8_t rule_violations(double length_in, double width_in, double height_in, int pins, int cups) {
    std::uint8_t v = 0;
    if (length_in > kMaxFootprintIn || width_in > kMaxFootprintIn)
        v |= kRuleSg2;
    if (height_in > kMaxHeightIn)
        v |= kRuleSg3;
    if (pins > kMaxPins || cups > kMaxCups)
        v |= kRuleSg6;
    return v;
}

struct RuleGuardConfig {
    // Höhe der höchsten Roboterstelle über dem Boden = gemessene Lifthöhe + dieser Versatz. TODO(HW) je Roboter in config/;
    // NaN = nicht gemessen: der Lift darf dann nie nach oben (sichere Seite).
    double lift_top_offset_in = std::numeric_limits<double>::quiet_NaN();
    // Abstand unter der harten Grenze, ab dem das Lift-Subsystem seine eigene Decke setzt (set_ceiling_in); die
    // Ausgabestufe sperrt erst an der harten Grenze, damit sie nicht gegen den Lageregler arbeitet.
    double soft_margin_in = 1.0;
};

struct GuardInput {
    double remaining_s = std::numeric_limits<double>::quiet_NaN();  // Restzeit des Matches; NaN = unbekannt
    bool lift_ok = false;                                           // Lift gehomed und Sensor ok
    double lift_height_in = 0.0;
    int pins = -1;  // Besitz; < 0 = unbekannt
    int cups = -1;
};

struct GuardCommand {
    double left_v = 0.0;
    double right_v = 0.0;
    double lift_v = 0.0;
    bool brake = false;  // true: Motoren bremsen (Hold)
};

struct GuardResult {
    GuardCommand cmd;
    std::uint8_t intervened = 0;  // RuleBit: welche Regel den Befehl verändert hat
};

enum class Grab { pin, cup };

class RuleGuard {
public:
    explicit RuleGuard(const RuleGuardConfig& cfg) : cfg_(cfg) {}

    // Höchste Lifthöhe, die SG3 erlaubt; -1e9 (nie erreichbar), wenn der Versatz fehlt.
    double hard_lift_ceiling_in() const {
        return std::isfinite(cfg_.lift_top_offset_in) ? kMaxHeightIn - cfg_.lift_top_offset_in : -1e9;
    }
    // Decke für LiftSubsystem::set_ceiling_in (unter der harten Grenze).
    double soft_lift_ceiling_in() const { return hard_lift_ceiling_in() - cfg_.soft_margin_in; }

    // SG6: Greifen nur, wenn die Zahl bekannt ist und noch Platz ist.
    bool may_grab(Grab what, const GuardInput& in) const {
        if (in.pins < 0 || in.cups < 0)
            return false;
        return what == Grab::pin ? in.pins < kMaxPins : in.cups < kMaxCups;
    }

    // Letzter Filter vor der HAL. Ab Restzeit <= 0 sind alle Ausgaben 0 V mit Bremse, und das bleibt so (Latch), auch
    // wenn die Restzeit später wieder größer gemeldet wird. Lift nach oben nur bei gehomtem Lift unter der harten Grenze.
    GuardResult filter(const GuardCommand& want, const GuardInput& in) {
        GuardResult r;
        r.cmd = want;
        if (std::isfinite(in.remaining_s) && in.remaining_s <= 0.0)
            ended_ = true;
        if (ended_) {
            r.cmd = GuardCommand{0.0, 0.0, 0.0, true};
            if (want.left_v != 0.0 || want.right_v != 0.0 || want.lift_v != 0.0 || !want.brake)
                r.intervened |= kRuleMatchEnd;
            return r;
        }
        if (want.lift_v > 0.0 && (!in.lift_ok || !(in.lift_height_in < hard_lift_ceiling_in()))) {
            r.cmd.lift_v = 0.0;
            r.intervened |= kRuleSg3;
        }
        if (in.pins > kMaxPins || in.cups > kMaxCups)
            r.intervened |= kRuleSg6;  // nur melden: Abbauen ist Sache des Zyklus (AUSWERFEN)
        return r;
    }

    bool match_ended() const { return ended_; }

private:
    RuleGuardConfig cfg_;
    bool ended_ = false;
};

}  // namespace frostbyte
