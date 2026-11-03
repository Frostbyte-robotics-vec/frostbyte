#include <limits>

#include "doctest/doctest.h"
#include "util/rule_guard.hpp"

using namespace frostbyte;

namespace {

// Testwerte, nicht aus config/: höchste Stelle = Lifthöhe + 20 Zoll.
RuleGuardConfig cfg() {
    RuleGuardConfig c;
    c.lift_top_offset_in = 20.0;
    c.soft_margin_in = 1.0;
    return c;
}

GuardInput ok_input() {
    GuardInput in;
    in.remaining_s = 30.0;
    in.lift_ok = true;
    in.lift_height_in = 10.0;
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
