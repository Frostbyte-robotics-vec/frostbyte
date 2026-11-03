#include <limits>

#include "doctest/doctest.h"
#include "util/rule_guard.hpp"

using namespace frostbyte;

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();

// Testwerte, nicht aus config/: höchste Stelle = Lifthöhe + 20 Zoll.
RuleGuardConfig cfg() {
    RuleGuardConfig c;
    c.lift_top_offset_in = 20.0;
    c.lift_footprint_ceiling_in = kInf;  // gemessen: der Lift verlässt den Footprint nie
    c.arm_min_deg = -10.0;
    c.arm_max_deg = 90.0;
    c.soft_margin_in = 1.0;
    return c;
}

GuardInput ok_input() {
    GuardInput in;
    in.remaining_s = 30.0;
    in.lift_ok = true;
    in.lift_height_in = 10.0;
    in.arm_ok = true;
    in.arm_deg = 45.0;
    in.pins = 0;
    in.cups = 0;
    return in;
}

GuardCommand moving() { return GuardCommand{6.0, 6.0, 4.0, false}; }

}  // namespace

TEST_CASE("SG2: 24 Zoll sind erlaubt, darüber nicht (Länge und Breite)") {
    CHECK(rule_violations(24.0, 24.0, 10.0, 0, 0) == 0);
    CHECK((rule_violations(24.01, 20.0, 10.0, 0, 0) & kRuleSg2) != 0);
    CHECK((rule_violations(20.0, 24.01, 10.0, 0, 0) & kRuleSg2) != 0);
}

TEST_CASE("SG3: 50 Zoll sind erlaubt, darüber nicht") {
    CHECK(rule_violations(15.0, 15.0, 50.0, 0, 0) == 0);
    CHECK((rule_violations(15.0, 15.0, 50.01, 0, 0) & kRuleSg3) != 0);
}

TEST_CASE("SG6: ein Pin und ein Cup sind erlaubt, ein zweites gleichartiges Objekt nicht; unbekannt meldet nichts") {
    CHECK(rule_violations(15.0, 15.0, 10.0, 1, 1) == 0);
    CHECK((rule_violations(15.0, 15.0, 10.0, 2, 1) & kRuleSg6) != 0);
    CHECK((rule_violations(15.0, 15.0, 10.0, 1, 2) & kRuleSg6) != 0);
    CHECK(rule_violations(15.0, 15.0, 10.0, -1, -1) == 0);
}

TEST_CASE("SG6: Greifen wird gesperrt, wenn schon ein gleichartiges Objekt gehalten wird, das andere bleibt erlaubt") {
    const RuleGuard g(cfg());
    GuardInput in = ok_input();
    CHECK(g.may_grab(Grab::pin, in));
    CHECK(g.may_grab(Grab::cup, in));
    in.pins = 1;
    CHECK_FALSE(g.may_grab(Grab::pin, in));
    CHECK(g.may_grab(Grab::cup, in));
    in.cups = 1;
    CHECK_FALSE(g.may_grab(Grab::cup, in));
}

TEST_CASE("SG6: unbekannter Besitz heißt nicht greifen") {
    const RuleGuard g(cfg());
    GuardInput in = ok_input();
    in.pins = -1;
    CHECK_FALSE(g.may_grab(Grab::pin, in));
    CHECK_FALSE(g.may_grab(Grab::cup, in));
}

TEST_CASE("SG3: harte Grenze 50 - Versatz; der Lift darf bis dahin nach oben, an der Grenze nicht mehr") {
    RuleGuard g(cfg());
    CHECK(g.hard_lift_ceiling_in() == doctest::Approx(30.0));
    CHECK(g.soft_lift_ceiling_in() == doctest::Approx(29.0));
    GuardInput in = ok_input();
    in.lift_height_in = 29.99;
    GuardResult r = g.filter(moving(), in);
    CHECK(r.cmd.lift_v == 4.0);
    CHECK(r.intervened == 0);
    in.lift_height_in = 30.0;
    r = g.filter(moving(), in);
    CHECK(r.cmd.lift_v == 0.0);
    CHECK((r.intervened & kRuleSg3) != 0);
    CHECK(r.cmd.left_v == 6.0);  // Antrieb bleibt unberührt
}

TEST_CASE("SG3: nach unten fahren bleibt über der Grenze erlaubt") {
    RuleGuard g(cfg());
    GuardInput in = ok_input();
    in.lift_height_in = 35.0;
    GuardCommand down = moving();
    down.lift_v = -3.0;
    const GuardResult r = g.filter(down, in);
    CHECK(r.cmd.lift_v == -3.0);
    CHECK(r.intervened == 0);
}

TEST_CASE("SG3: Lift nicht gehomed oder Sensor nicht ok sperrt nur nach oben") {
    RuleGuard g(cfg());
    GuardInput in = ok_input();
    in.lift_ok = false;
    GuardResult r = g.filter(moving(), in);
    CHECK(r.cmd.lift_v == 0.0);
    GuardCommand down = moving();
    down.lift_v = -2.0;
    CHECK(g.filter(down, in).cmd.lift_v == -2.0);
}

TEST_CASE("SG3: fehlender Höhenversatz (nicht gemessen) sperrt den Lift nach oben immer") {
    RuleGuardConfig c;  // NaN
    RuleGuard g(c);
    GuardInput in = ok_input();
    CHECK(g.filter(moving(), in).cmd.lift_v == 0.0);
}

TEST_CASE("Matchende: bei Restzeit 0 sind alle Ausgaben 0 V mit Bremse und bleiben es (Latch)") {
    RuleGuard g(cfg());
    GuardInput in = ok_input();
    CHECK_FALSE(g.filter(moving(), in).cmd.brake);
    in.remaining_s = 0.0;
    GuardResult r = g.filter(moving(), in);
    CHECK(r.cmd.left_v == 0.0);
    CHECK(r.cmd.right_v == 0.0);
    CHECK(r.cmd.lift_v == 0.0);
    CHECK(r.cmd.brake);
    CHECK((r.intervened & kRuleMatchEnd) != 0);
    CHECK(g.match_ended());
    in.remaining_s = 5.0;  // darf nie wieder anlaufen
    r = g.filter(moving(), in);
    CHECK(r.cmd.left_v == 0.0);
    CHECK(r.cmd.brake);
}

TEST_CASE("Matchende: ein Haltbefehl bei 0:00 ist kein Eingriff") {
    RuleGuard g(cfg());
    GuardInput in = ok_input();
    in.remaining_s = -0.01;
    const GuardResult r = g.filter(GuardCommand{0.0, 0.0, 0.0, true}, in);
    CHECK(r.intervened == 0);
    CHECK(r.cmd.brake);
}

TEST_CASE("Matchende: unbekannte Restzeit (NaN) löst nichts aus") {
    RuleGuard g(cfg());
    GuardInput in = ok_input();
    in.remaining_s = std::numeric_limits<double>::quiet_NaN();
    CHECK(g.filter(moving(), in).cmd.left_v == 6.0);
    CHECK_FALSE(g.match_ended());
}

TEST_CASE("Ein Befehl innerhalb aller Grenzen geht unverändert durch") {
    RuleGuard g(cfg());
    const GuardResult r = g.filter(moving(), ok_input());
    CHECK(r.intervened == 0);
    CHECK(r.cmd.left_v == 6.0);
    CHECK(r.cmd.right_v == 6.0);
    CHECK(r.cmd.lift_v == 4.0);
    CHECK_FALSE(r.cmd.brake);
}

TEST_CASE("SG6: Überbesitz wird gemeldet, der Befehl bleibt (Abbauen macht der Zyklus)") {
    RuleGuard g(cfg());
    GuardInput in = ok_input();
    in.pins = 2;
    const GuardResult r = g.filter(moving(), in);
    CHECK((r.intervened & kRuleSg6) != 0);
    CHECK(r.cmd.left_v == 6.0);
}

TEST_CASE("Disabled: alles 0 V mit Bremse, ohne Latch; nach Enable läuft es wieder") {
    RuleGuard g(cfg());
    GuardInput in = ok_input();
    in.disabled = true;
    GuardResult r = g.filter(moving(), in);
    CHECK(r.cmd.left_v == 0.0);
    CHECK(r.cmd.right_v == 0.0);
    CHECK(r.cmd.lift_v == 0.0);
    CHECK(r.cmd.brake);
    CHECK((r.intervened & kRuleDisabled) != 0);
    in.disabled = false;
    r = g.filter(moving(), in);
    CHECK(r.cmd.left_v == 6.0);
    CHECK(r.intervened == 0);
    CHECK_FALSE(g.match_ended());
}

// ---- M-09: SG2/SG3 aus der Geometrie (ADR-027, Nachtrag). Alle Zahlen sind Testwerte, nichts aus config/. ----

namespace {

GuardCommand lift_up() { return GuardCommand{.lift_v = 4.0}; }
GuardCommand arm_cmd(double v) { return GuardCommand{.arm_v = v}; }

}  // namespace

TEST_CASE("SG2 Lift: Footprint-Decke unter der SG3-Decke bindet, knapp darunter erlaubt, nach unten immer frei") {
    RuleGuardConfig c = cfg();
    c.lift_top_offset_in = 10.0;       // SG3-Decke 40
    c.lift_footprint_ceiling_in = 30;  // SG2-Decke 30 bindet
    RuleGuard g(c);
    CHECK(g.hard_lift_ceiling_in() == 30.0);
    GuardInput in = ok_input();
    in.lift_height_in = 29.99;
    CHECK(g.filter(lift_up(), in).cmd.lift_v == 4.0);
    in.lift_height_in = 30.0;
    GuardResult r = g.filter(lift_up(), in);
    CHECK(r.cmd.lift_v == 0.0);
    CHECK((r.intervened & kRuleSg2) != 0);
    CHECK((r.intervened & kRuleSg3) == 0);
    in.lift_height_in = 35.0;  // über der Decke: nach unten bleibt erlaubt
    CHECK(g.filter(GuardCommand{.lift_v = -4.0}, in).cmd.lift_v == -4.0);
}

TEST_CASE("SG3 Lift: bindet die SG3-Decke (unter der Footprint-Decke), wird SG3 gemeldet, nicht SG2") {
    RuleGuardConfig c = cfg();
    c.lift_top_offset_in = 10.0;       // SG3-Decke 40
    c.lift_footprint_ceiling_in = 45;  // darüber
    RuleGuard g(c);
    CHECK(g.hard_lift_ceiling_in() == 40.0);
    GuardInput in = ok_input();
    in.lift_height_in = 39.99;
    CHECK(g.filter(lift_up(), in).cmd.lift_v == 4.0);
    in.lift_height_in = 40.0;
    const GuardResult r = g.filter(lift_up(), in);
    CHECK(r.cmd.lift_v == 0.0);
    CHECK((r.intervened & kRuleSg3) != 0);
    CHECK((r.intervened & kRuleSg2) == 0);
}

TEST_CASE("SG2 Lift: nicht gemessen (NaN) sperrt nach oben, gemessen ohne Grenze (+inf) nicht; weiche Decke folgt") {
    RuleGuardConfig c = cfg();
    c.lift_footprint_ceiling_in = kNan;
    CHECK(RuleGuard(c).filter(lift_up(), ok_input()).cmd.lift_v == 0.0);
    c.lift_footprint_ceiling_in = kInf;
    CHECK(RuleGuard(c).filter(lift_up(), ok_input()).cmd.lift_v == 4.0);
    c.lift_top_offset_in = 10.0;
    c.lift_footprint_ceiling_in = 30.0;
    c.soft_margin_in = 1.0;
    CHECK(RuleGuard(c).soft_lift_ceiling_in() == 29.0);  // min(40, 30) - 1
}

TEST_CASE("SG2/SG3 Arm: Grenzen einschließlich, nur die Richtung nach außen wird gesperrt") {
    RuleGuard g(cfg());  // Bereich -10 .. 90
    GuardInput in = ok_input();
    in.arm_deg = 89.99;
    CHECK(g.filter(arm_cmd(3.0), in).cmd.arm_v == 3.0);
    in.arm_deg = 90.0;
    GuardResult r = g.filter(arm_cmd(3.0), in);
    CHECK(r.cmd.arm_v == 0.0);
    CHECK((r.intervened & kRuleSg2) != 0);
    CHECK((r.intervened & kRuleSg3) != 0);
    CHECK(g.filter(arm_cmd(-3.0), in).cmd.arm_v == -3.0);  // nach innen frei
    in.arm_deg = -9.99;
    CHECK(g.filter(arm_cmd(-3.0), in).cmd.arm_v == -3.0);
    in.arm_deg = -10.0;
    CHECK(g.filter(arm_cmd(-3.0), in).cmd.arm_v == 0.0);
    CHECK(g.filter(arm_cmd(3.0), in).cmd.arm_v == 3.0);
    in.arm_deg = 95.0;  // über die Grenze hinaus (Überschwinger): zurück ist erlaubt, weiter nicht
    CHECK(g.filter(arm_cmd(3.0), in).cmd.arm_v == 0.0);
    CHECK(g.filter(arm_cmd(-3.0), in).cmd.arm_v == -3.0);
}

TEST_CASE(
    "Arm: unbekannter Zustand oder nicht gemessene Grenzen sperren beide Richtungen; Stillstand ist kein Eingriff") {
    RuleGuard g(cfg());
    GuardInput in = ok_input();
    in.arm_ok = false;
    CHECK(g.filter(arm_cmd(3.0), in).cmd.arm_v == 0.0);
    CHECK(g.filter(arm_cmd(-3.0), in).cmd.arm_v == 0.0);
    CHECK(g.filter(arm_cmd(0.0), in).intervened == 0);
    in.arm_ok = true;
    in.arm_deg = kNan;
    CHECK(g.filter(arm_cmd(3.0), in).cmd.arm_v == 0.0);
    CHECK(g.filter(arm_cmd(-3.0), in).cmd.arm_v == 0.0);

    RuleGuardConfig c = cfg();
    c.arm_min_deg = kNan;
    c.arm_max_deg = kNan;
    RuleGuard unmeasured(c);
    in.arm_deg = 45.0;
    CHECK(unmeasured.filter(arm_cmd(3.0), in).cmd.arm_v == 0.0);
    CHECK(unmeasured.filter(arm_cmd(-3.0), in).cmd.arm_v == 0.0);
    CHECK(RuleGuard(RuleGuardConfig{}).filter(arm_cmd(3.0), in).cmd.arm_v == 0.0);  // Standardkonfiguration
}

TEST_CASE(
    "Arm: Disabled und Matchende (mit Latch) stellen auch den Arm still, eine Haltbefehl-Hülle bleibt kein Eingriff") {
    RuleGuard g(cfg());
    GuardInput in = ok_input();
    in.disabled = true;
    GuardResult r = g.filter(arm_cmd(3.0), in);
    CHECK(r.cmd.arm_v == 0.0);
    CHECK(r.cmd.brake);
    CHECK((r.intervened & kRuleDisabled) != 0);
    in.disabled = false;
    CHECK(g.filter(arm_cmd(3.0), in).cmd.arm_v == 3.0);
    in.remaining_s = 0.0;
    r = g.filter(arm_cmd(3.0), in);
    CHECK(r.cmd.arm_v == 0.0);
    CHECK((r.intervened & kRuleMatchEnd) != 0);
    in.remaining_s = 5.0;  // Latch
    CHECK(g.filter(arm_cmd(3.0), in).cmd.arm_v == 0.0);
    // Nur ein Arm-Befehl ohne Bremse ist bei Matchende ein Eingriff
    RuleGuard h(cfg());
    in = ok_input();
    in.remaining_s = 0.0;
    CHECK(h.filter(GuardCommand{.brake = true}, in).intervened == 0);
    CHECK(h.filter(GuardCommand{.brake = true, .arm_v = 1.0}, in).intervened != 0);  // nur der Arm bewegt sich
}
