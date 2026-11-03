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
enum RuleBit : std::uint8_t { kRuleSg2 = 1, kRuleSg3 = 2, kRuleSg6 = 4, kRuleMatchEnd = 8, kRuleDisabled = 16 };

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
    // SG2: Lifthöhe, ab der der Footprint 24" überschreitet (Lift kippt oder fährt aus). NaN = nicht gemessen: der Lift darf
    // dann nie nach oben. +inf = gemessen, der Lift verlässt den Footprint nie (ADR-027, Nachtrag M-09).
    double lift_footprint_ceiling_in = std::numeric_limits<double>::quiet_NaN();
    // Arm: Winkelbereich, in dem Footprint <= 24" und Höhe <= 50" bleiben (Worst Case über die Lifthöhe). Positive
    // arm_v erhöht arm_deg; die physische Richtung steht in HAL/config. NaN = nicht gemessen: Arm in beide Richtungen gesperrt.
    double arm_min_deg = std::numeric_limits<double>::quiet_NaN();
    double arm_max_deg = std::numeric_limits<double>::quiet_NaN();
    // Abstand unter der harten Grenze, ab dem das Lift-Subsystem seine eigene Decke setzt (set_ceiling_in); die
    // Ausgabestufe sperrt erst an der harten Grenze, damit sie nicht gegen den Lageregler arbeitet.
    double soft_margin_in = 1.0;
};

struct GuardInput {
    double remaining_s = std::numeric_limits<double>::quiet_NaN();  // Restzeit des Matches; NaN = unbekannt
    bool lift_ok = false;                                           // Lift gehomed und Sensor ok
    double lift_height_in = 0.0;
    bool arm_ok = false;  // Arm gehomed und Sensor ok
    double arm_deg = std::numeric_limits<double>::quiet_NaN();
    bool disabled = false;  // Phase Disabled (Feld oder Competition-Schalter): Halt, aber ohne Latch
    int pins = -1;          // Besitz; < 0 = unbekannt
    int cups = -1;
};

struct GuardCommand {
    double left_v = 0.0;
    double right_v = 0.0;
    double lift_v = 0.0;
    bool brake = false;  // true: Motoren bremsen (Hold)
    double arm_v = 0.0;  // hinter brake: bestehende Initialisierungen mit vier Werten bleiben gültig
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
    double sg3_lift_ceiling_in() const {
        return std::isfinite(cfg_.lift_top_offset_in) ? kMaxHeightIn - cfg_.lift_top_offset_in : -1e9;
    }
    // Höchste Lifthöhe, die SG2 erlaubt; -1e9, wenn nicht gemessen (NaN). +inf = nie ein Problem.
    double sg2_lift_ceiling_in() const {
        return std::isnan(cfg_.lift_footprint_ceiling_in) ? -1e9 : cfg_.lift_footprint_ceiling_in;
    }
    // Höchste Lifthöhe, die SG2 und SG3 zusammen erlauben.
    double hard_lift_ceiling_in() const { return std::fmin(sg3_lift_ceiling_in(), sg2_lift_ceiling_in()); }
    // Decke für LiftSubsystem::set_ceiling_in (unter der harten Grenze).
    double soft_lift_ceiling_in() const { return hard_lift_ceiling_in() - cfg_.soft_margin_in; }

    // SG6: Greifen nur, wenn die Zahl bekannt ist und noch Platz ist.
    bool may_grab(Grab what, const GuardInput& in) const {
        if (in.pins < 0 || in.cups < 0)
            return false;
        return what == Grab::pin ? in.pins < kMaxPins : in.cups < kMaxCups;
    }

    // Letzter Filter vor der HAL. Disabled: alles 0 V mit Bremse, ohne Latch (nach Enable läuft es wieder). Ab Restzeit <= 0 sind alle Ausgaben 0 V mit Bremse, und das bleibt so (Latch), auch
    // wenn die Restzeit später wieder größer gemeldet wird. Lift nach oben nur bei gehomtem Lift unter der harten Grenze.
    GuardResult filter(const GuardCommand& want, const GuardInput& in) {
        GuardResult r;
        r.cmd = want;
        if (std::isfinite(in.remaining_s) && in.remaining_s <= 0.0)
            ended_ = true;
        if (!ended_ && in.disabled) {
            r.cmd = GuardCommand{.brake = true};
            if (moving(want))
                r.intervened |= kRuleDisabled;
            return r;
        }
        if (ended_) {
            r.cmd = GuardCommand{.brake = true};
            if (moving(want))
                r.intervened |= kRuleMatchEnd;
            return r;
        }
        if (want.lift_v > 0.0) {
            // Nicht-Vergleiche (NaN Höhe) sperren; gemeldet wird die Regel, deren Decke erreicht ist.
            const bool sg3 = !(in.lift_height_in < sg3_lift_ceiling_in());
            const bool sg2 = !(in.lift_height_in < sg2_lift_ceiling_in());
            if (!in.lift_ok || sg3 || sg2) {
                r.cmd.lift_v = 0.0;
                r.intervened |= (sg2 ? kRuleSg2 : 0) | ((sg3 || !in.lift_ok) ? kRuleSg3 : 0);
            }
        }
        // Arm: nur die Richtung nach außen wird gesperrt; unbekannter Zustand oder nicht gemessene Grenzen sperren beide
        // (Vergleiche mit NaN sind falsch). Die Grenzen gelten für SG2 und SG3 zusammen, gemeldet werden beide.
        if (want.arm_v != 0.0) {
            const bool known = in.arm_ok && std::isfinite(in.arm_deg);
            const bool allowed =
                known && (want.arm_v > 0.0 ? in.arm_deg < cfg_.arm_max_deg : in.arm_deg > cfg_.arm_min_deg);
            if (!allowed) {
                r.cmd.arm_v = 0.0;
                r.intervened |= kRuleSg2 | kRuleSg3;
            }
        }
        if (in.pins > kMaxPins || in.cups > kMaxCups)
            r.intervened |= kRuleSg6;  // nur melden: Abbauen ist Sache des Zyklus (AUSWERFEN)
        return r;
    }

    bool match_ended() const { return ended_; }

private:
    // Hat der Befehl etwas bewegt oder die Bremse nicht gesetzt? (Maßstab für "Eingriff" bei Halt)
    static bool moving(const GuardCommand& c) {
        return c.left_v != 0.0 || c.right_v != 0.0 || c.lift_v != 0.0 || c.arm_v != 0.0 || !c.brake;
    }

    RuleGuardConfig cfg_;
    bool ended_ = false;
};

}  // namespace frostbyte
