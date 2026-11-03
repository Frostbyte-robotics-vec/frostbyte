#include <cmath>
#include <cstring>
#include <limits>

#include "doctest/doctest.h"
#include "tasks/cycle_controller.hpp"

using namespace frostbyte;

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// Testzahlen (nicht aus field.json oder config/): Goal G0 auf 18", Stufe 4", Greifer-Versatz 3", Roboter-Oberkante = Lifthöhe + 10"
// -> harte Lift-Grenze 40", weiche Decke 39".
GoalBook make_book() {
    std::array<GoalEntry, GoalBook::kGoals> g{};
    static const char* names[GoalBook::kGoals] = {"G0", "G1", "G2", "G3", "G4", "G5", "G6", "G7", "TALL"};
    for (int i = 0; i < GoalBook::kGoals; ++i)
        g[i] = GoalEntry{names[i], 18.0};
    g[8].base_in = 38.0;
    GoalBook b;
    REQUIRE(b.set(g, 4.0));
    return b;
}

RuleGuardConfig guard_config() {
    RuleGuardConfig c;
    c.lift_top_offset_in = 10.0;
    c.soft_margin_in = 1.0;
    return c;
}

CycleConfig cycle_config() {
    CycleConfig c;
    c.pick_timeout_s = 4.0;
    c.grasp_timeout_s = 1.5;
    c.lift_timeout_s = 3.0;
    c.approach_timeout_s = 5.0;
    c.place_timeout_s = 1.5;
    c.retreat_timeout_s = 2.0;
    c.retreat_lift_in = 12.0;
    c.grip_offset_in = 3.0;
    return c;
}

struct Rig {
    GoalBook book = make_book();
    RuleGuard guard{guard_config()};
    CycleController cc;
    double t = 0.0;
    CycleInputs in;
    CycleCommand cmd;

    explicit Rig(const CycleConfig& cfg = cycle_config()) : cc(cfg, book, guard, 0.0) {}

    CycleRefusal start(CycleSource src = CycleSource::quiver, const char* goal = "G0", int level = 2,
                       double timeout_s = -1.0, int pins = 0, int cups = 0) {
        return cc.request_cycle(CycleRequest{goal, level, src, timeout_s}, pins, cups, t);
    }
    // Ein Takt von 10 ms mit den aktuellen Eingängen.
    void tick() {
        t += 0.01;
        cmd = cc.update(t, in);
    }
    // Zeit vorspulen (Eingänge unverändert), bis dt_s vergangen ist.
    void run(double dt_s) {
        const double end = t + dt_s;
        while (t < end - 1e-9)
            tick();
    }
    // Vorwärts, bis der Controller in 'target' ist (höchstens 100 s).
    void run_until(CycleState target) {
        for (int i = 0; i < 10000 && cc.state() != target; ++i)
            tick();
        REQUIRE(cc.state() == target);
    }
    // Einen Auslöser für einen Takt setzen und wieder zurücknehmen.
    void pulse(bool CycleInputs::*flag) {
        in.*flag = true;
        tick();
        in.*flag = false;
    }
    // Bis in den gewünschten Zustand fahren (Quiver-Ablauf), mit den passenden Auslösern.
    void advance_to(CycleState target) {
        if (cc.state() == CycleState::picking && target != CycleState::picking) {
            pulse(&CycleInputs::unit_present);
        }
        if (cc.state() == CycleState::grasping && target != CycleState::grasping) {
            pulse(&CycleInputs::claw_closed);
        }
        if (cc.state() == CycleState::lifting && target != CycleState::lifting) {
            pulse(&CycleInputs::lift_at_target);
        }
        if (cc.state() == CycleState::at_goal && target != CycleState::at_goal) {
            pulse(&CycleInputs::at_goal);
        }
        if (cc.state() == CycleState::placing && target != CycleState::placing) {
            pulse(&CycleInputs::claw_open);
        }
        if (cc.state() == CycleState::retreating && target != CycleState::retreating) {
            pulse(&CycleInputs::retreat_done);
        }
        if (cc.state() != target && cc.state() != CycleState::ready)
            advance_to(target);
    }
};

}  // namespace

TEST_CASE("Zustandsnamen entsprechen genau docs/log_format.md") {
    CHECK(std::strcmp(cycle_state_name(CycleState::ready), "BEREIT") == 0);
    CHECK(std::strcmp(cycle_state_name(CycleState::picking), "AUFNEHMEN") == 0);
    CHECK(std::strcmp(cycle_state_name(CycleState::grasping), "GREIFEN") == 0);
    CHECK(std::strcmp(cycle_state_name(CycleState::lifting), "HEBEN") == 0);
    CHECK(std::strcmp(cycle_state_name(CycleState::at_goal), "AM_GOAL") == 0);
    CHECK(std::strcmp(cycle_state_name(CycleState::placing), "SETZEN") == 0);
    CHECK(std::strcmp(cycle_state_name(CycleState::retreating), "RUECKZUG") == 0);
    CHECK(std::strcmp(cycle_state_name(CycleState::error), "FEHLER") == 0);
}

TEST_CASE("Anfangszustand BEREIT, alles aus") {
    Rig r;
    CHECK(r.cc.state() == CycleState::ready);
    r.tick();
    CHECK(r.cmd.drive == CycleDrive::stop);
    CHECK_FALSE(r.cmd.intake_on);
    CHECK(r.cmd.claw == CycleClaw::hold);
    CHECK(std::isnan(r.cmd.lift_target_in));
    CHECK_FALSE(r.cc.result().finished);
}

TEST_CASE("Quiver: voller Ablauf, Ausgaben je Zustand") {
    Rig r;
    REQUIRE(r.start(CycleSource::quiver, "G0", 2) == CycleRefusal::none);
    CHECK(r.cc.state() == CycleState::picking);

    r.tick();
    CHECK(r.cmd.drive == CycleDrive::to_source);
    CHECK(r.cmd.intake_on);
    CHECK(r.cmd.claw == CycleClaw::hold);
    CHECK(std::isnan(r.cmd.lift_target_in));
    CHECK(r.cc.state() == CycleState::picking);  // ohne Auslöser keine Bewegung

    r.pulse(&CycleInputs::unit_present);
    CHECK(r.cc.state() == CycleState::grasping);
    r.tick();
    CHECK(r.cmd.drive == CycleDrive::stop);
    CHECK_FALSE(r.cmd.intake_on);
    CHECK(r.cmd.claw == CycleClaw::close);

    r.pulse(&CycleInputs::claw_closed);
    CHECK(r.cc.state() == CycleState::lifting);
    r.tick();
    CHECK(r.cmd.claw == CycleClaw::close);
    CHECK(r.cmd.lift_target_in == doctest::Approx(18.0 + 2 * 4.0 + 3.0));  // Goal + 2 Stufen + Greifer-Versatz
    CHECK(r.cmd.drive == CycleDrive::stop);

    r.pulse(&CycleInputs::lift_at_target);
    CHECK(r.cc.state() == CycleState::at_goal);
    r.tick();
    CHECK(r.cmd.drive == CycleDrive::to_goal);
    CHECK(r.cmd.claw == CycleClaw::close);
    CHECK(r.cmd.lift_target_in == doctest::Approx(29.0));

    r.pulse(&CycleInputs::at_goal);
    CHECK(r.cc.state() == CycleState::placing);
    r.tick();
    CHECK(r.cmd.drive == CycleDrive::stop);
    CHECK(r.cmd.claw == CycleClaw::open);
    CHECK(r.cmd.lift_target_in == doctest::Approx(29.0));
    CHECK_FALSE(r.cc.result().placed);

    r.pulse(&CycleInputs::claw_open);
    CHECK(r.cc.state() == CycleState::retreating);
    CHECK(r.cc.result().placed);
    CHECK_FALSE(r.cc.result().finished);
    r.tick();
    CHECK(r.cmd.drive == CycleDrive::retreat);
    CHECK(r.cmd.claw == CycleClaw::hold);
    CHECK(r.cmd.lift_target_in == doctest::Approx(12.0));

    r.pulse(&CycleInputs::retreat_done);
    CHECK(r.cc.state() == CycleState::ready);
    const CycleResult res = r.cc.result();
    CHECK(res.finished);
    CHECK(res.placed);
    CHECK(res.ok());
    CHECK_FALSE(res.timed_out);
    CHECK_FALSE(res.cancelled);
    r.tick();
    CHECK(r.cmd.drive == CycleDrive::stop);
}

TEST_CASE("Preload: AUFNEHMEN entfällt, may_grab wird nicht geprüft") {
    Rig r;
    // Besitz voll (1 Pin + 1 Cup): ein Quiver-Zyklus wird abgelehnt, ein Preload-Zyklus nicht.
    CHECK(r.start(CycleSource::quiver, "G0", 0, -1.0, 1, 1) == CycleRefusal::cannot_grab);
    REQUIRE(r.start(CycleSource::preload, "G0", 0, -1.0, 1, 1) == CycleRefusal::none);
    CHECK(r.cc.state() == CycleState::grasping);
    r.tick();
    CHECK(r.cmd.claw == CycleClaw::close);
    CHECK_FALSE(r.cmd.intake_on);
    CHECK(r.cmd.drive == CycleDrive::stop);
    r.pulse(&CycleInputs::claw_closed);
    CHECK(r.cc.state() == CycleState::lifting);
}

TEST_CASE("Timeouts: jeder laufende Zustand fällt nach FEHLER, mit Grund im Ergebnis") {
    struct Row {
        CycleState state;
        double timeout_s;
    };
    const Row rows[] = {{CycleState::picking, 4.0},
                        {CycleState::grasping, 1.5},
                        {CycleState::lifting, 3.0},
                        {CycleState::at_goal, 5.0},
                        {CycleState::placing, 1.5}};
    for (const Row& row : rows) {
        CAPTURE(cycle_state_name(row.state));
        Rig r;
        REQUIRE(r.start() == CycleRefusal::none);
        r.advance_to(row.state);
        REQUIRE(r.cc.state() == row.state);
        // Knapp vor dem Timeout: noch im Zustand.
        r.run(row.timeout_s - 0.05);
        CHECK(r.cc.state() == row.state);
        // Danach: FEHLER.
        r.run(0.1);
        CHECK(r.cc.state() == CycleState::error);
        const CycleResult res = r.cc.result();
        CHECK(res.finished);
        CHECK(res.timed_out);
        CHECK(res.timeout_state == row.state);
        CHECK_FALSE(res.placed);
        CHECK_FALSE(res.ok());
    }
}

TEST_CASE("Auslöser und Timeout im selben Takt: der Auslöser gewinnt") {
    Rig r;
    REQUIRE(r.start() == CycleRefusal::none);
    // Genau am Timeout (4,0 s nach dem Start bei t = 0) meldet der Bumper eine Einheit.
    r.in.unit_present = true;
    r.t = 4.0;
    r.cmd = r.cc.update(r.t, r.in);
    CHECK(r.cc.state() == CycleState::grasping);
    CHECK_FALSE(r.cc.result().timed_out);
}

TEST_CASE("FEHLER ist passiv und bleibt, bis ein neuer Befehl oder cancel kommt") {
    Rig r;
    REQUIRE(r.start() == CycleRefusal::none);
    r.advance_to(CycleState::lifting);
    r.run(3.1);
    REQUIRE(r.cc.state() == CycleState::error);
    r.tick();
    CHECK(r.cmd.drive == CycleDrive::stop);
    CHECK_FALSE(r.cmd.intake_on);
    CHECK(r.cmd.claw == CycleClaw::hold);
    CHECK(std::isnan(r.cmd.lift_target_in));
    // Auch bei gemeldeten Auslösern und langer Zeit bleibt FEHLER.
    r.in.unit_present = r.in.claw_closed = r.in.lift_at_target = r.in.at_goal = r.in.claw_open = r.in.retreat_done =
        true;
    r.run(60.0);
    CHECK(r.cc.state() == CycleState::error);
    CHECK(r.cmd.drive == CycleDrive::stop);
}

TEST_CASE("Aus FEHLER startet ein neuer Befehl; cancel führt nach BEREIT") {
    Rig r;
    REQUIRE(r.start() == CycleRefusal::none);
    r.run(4.1);
    REQUIRE(r.cc.state() == CycleState::error);
    REQUIRE(r.start(CycleSource::preload) == CycleRefusal::none);
    CHECK(r.cc.state() == CycleState::grasping);
    CHECK_FALSE(r.cc.result().finished);  // altes Ergebnis ist gelöscht

    Rig r2;
    REQUIRE(r2.start() == CycleRefusal::none);
    r2.run(4.1);
    REQUIRE(r2.cc.state() == CycleState::error);
    r2.cc.cancel(r2.t);
    CHECK(r2.cc.state() == CycleState::ready);
    r2.tick();
    CHECK(r2.cmd.drive == CycleDrive::stop);
}

TEST_CASE("Timeout im RUECKZUG ist kein Fehlschlag: gesetzt, aber mit Timeout") {
    Rig r;
    REQUIRE(r.start() == CycleRefusal::none);
    r.advance_to(CycleState::retreating);
    REQUIRE(r.cc.state() == CycleState::retreating);
    r.run(1.95);
    CHECK(r.cc.state() == CycleState::retreating);
    r.run(0.1);
    CHECK(r.cc.state() == CycleState::ready);
    const CycleResult res = r.cc.result();
    CHECK(res.finished);
    CHECK(res.placed);
    CHECK(res.ok());
    CHECK(res.timed_out);
    CHECK(res.timeout_state == CycleState::retreating);
    r.tick();
    CHECK(r.cmd.drive == CycleDrive::stop);
}

TEST_CASE("Timeout für den ganzen Zyklus") {
    {
        Rig r;
        REQUIRE(r.start(CycleSource::quiver, "G0", 2, 6.0) == CycleRefusal::none);
        r.advance_to(CycleState::lifting);  // Zeit vergeht nur über Takte, hier ein paar
        r.run(2.0);                         // einzelne Zustände bleiben unter ihrem eigenen Timeout
        r.pulse(&CycleInputs::lift_at_target);
        REQUIRE(r.cc.state() == CycleState::at_goal);
        r.run(5.0);  // approach_timeout_s = 5.0, aber der Gesamt-Timeout (6 s) trifft vorher
        CHECK(r.cc.state() == CycleState::error);
        CHECK(r.cc.result().timed_out);
        CHECK(r.cc.result().timeout_state == CycleState::at_goal);
        CHECK_FALSE(r.cc.result().placed);
    }
    {
        // Gesamt-Timeout nach dem Setzen: gesetzt, Ergebnis ok mit Timeout, Zustand BEREIT.
        Rig r;
        REQUIRE(r.start(CycleSource::quiver, "G0", 2, 1.0) == CycleRefusal::none);
        r.advance_to(CycleState::retreating);
        REQUIRE(r.cc.state() == CycleState::retreating);
        r.run(1.1);
        CHECK(r.cc.state() == CycleState::ready);
        CHECK(r.cc.result().placed);
        CHECK(r.cc.result().timed_out);
        CHECK(r.cc.result().timeout_state == CycleState::retreating);
    }
    {
        // Kein Gesamt-Timeout (< 0): nur die Zustands-Timeouts.
        Rig r;
        REQUIRE(r.start(CycleSource::quiver, "G0", 2, -1.0) == CycleRefusal::none);
        r.run(3.9);
        CHECK(r.cc.state() == CycleState::picking);
    }
}

TEST_CASE("cancel aus jedem laufenden Zustand: BEREIT, Antrieb steht, Lift hält") {
    const CycleState states[] = {CycleState::picking, CycleState::grasping, CycleState::lifting,
                                 CycleState::at_goal, CycleState::placing,  CycleState::retreating};
    for (CycleState s : states) {
        CAPTURE(cycle_state_name(s));
        Rig r;
        REQUIRE(r.start() == CycleRefusal::none);
        r.advance_to(s);
        REQUIRE(r.cc.state() == s);
        r.cc.cancel(r.t);
        CHECK(r.cc.state() == CycleState::ready);
        const CycleResult res = r.cc.result();
        CHECK(res.finished);
        CHECK(res.cancelled);
        CHECK_FALSE(res.timed_out);
        CHECK_FALSE(res.ok());
        r.tick();
        CHECK(r.cmd.drive == CycleDrive::stop);
        CHECK_FALSE(r.cmd.intake_on);
        CHECK(r.cmd.claw == CycleClaw::hold);
        CHECK(std::isnan(r.cmd.lift_target_in));
    }
}

TEST_CASE("cancel nach dem Setzen behält placed; cancel in BEREIT ändert nichts") {
    Rig r;
    REQUIRE(r.start() == CycleRefusal::none);
    r.advance_to(CycleState::retreating);
    r.cc.cancel(r.t);
    CHECK(r.cc.result().placed);
    CHECK(r.cc.result().cancelled);
    CHECK_FALSE(r.cc.result().ok());

    Rig r2;
    r2.cc.cancel(r2.t);  // nichts läuft
    CHECK(r2.cc.state() == CycleState::ready);
    CHECK_FALSE(r2.cc.result().finished);
}

TEST_CASE("Ablehnungen lassen den Zustand unverändert") {
    Rig r;
    CHECK(r.start(CycleSource::quiver, "NOPE", 0) == CycleRefusal::goal_unknown);
    CHECK(r.start(CycleSource::quiver, "G0", -1) == CycleRefusal::level_invalid);  // auto löst der RoutePlayer auf
    CHECK(r.start(CycleSource::quiver, "G0", 6) == CycleRefusal::level_invalid);
    CHECK(r.start(CycleSource::quiver, "TALL", 0) == CycleRefusal::too_high);                // 38 + 3 = 41 > 39
    CHECK(r.start(CycleSource::quiver, "G0", 0, -1.0, -1, 0) == CycleRefusal::cannot_grab);  // Besitz unbekannt
    CHECK(r.start(CycleSource::quiver, "G0", 0, -1.0, 1, 1) == CycleRefusal::cannot_grab);   // voll
    CHECK(r.cc.state() == CycleState::ready);
    CHECK_FALSE(r.cc.result().finished);
    // Platz für nur eine Art genügt.
    CHECK(r.start(CycleSource::quiver, "G0", 0, -1.0, 1, 0) == CycleRefusal::none);
}

TEST_CASE("Ablehnung: Höhe unbekannt, Versatz unbekannt, Konfiguration ungültig, belegt") {
    {
        // Ohne Stufe sind Ebenen > 0 unbekannt (wie field.json vor C-06).
        std::array<GoalEntry, GoalBook::kGoals> g{};
        static const char* names[GoalBook::kGoals] = {"G0", "G1", "G2", "G3", "G4", "G5", "G6", "G7", "G8"};
        for (int i = 0; i < GoalBook::kGoals; ++i)
            g[i] = GoalEntry{names[i], 18.0};
        GoalBook b;
        REQUIRE(b.set(g, kNaN));
        RuleGuard guard(guard_config());
        CycleController cc(cycle_config(), b, guard, 0.0);
        CHECK(cc.request_cycle(CycleRequest{"G0", 1, CycleSource::preload, -1.0}, 0, 0, 0.0) ==
              CycleRefusal::level_unknown);
        CHECK(cc.request_cycle(CycleRequest{"G0", 0, CycleSource::preload, -1.0}, 0, 0, 0.0) == CycleRefusal::none);
    }
    {
        CycleConfig cfg = cycle_config();
        cfg.grip_offset_in = kNaN;
        Rig r(cfg);
        CHECK(r.start() == CycleRefusal::offset_unknown);
    }
    {
        // Fehlt der Versatz im RuleGuard (NaN), ist die Decke nie erreichbar.
        GoalBook b = make_book();
        RuleGuard guard{RuleGuardConfig{}};
        CycleController cc(cycle_config(), b, guard, 0.0);
        CHECK(cc.request_cycle(CycleRequest{"G0", 0, CycleSource::preload, -1.0}, 0, 0, 0.0) == CycleRefusal::too_high);
    }
    {
        CycleConfig cfg = cycle_config();
        cfg.place_timeout_s = 0.0;
        Rig r(cfg);
        CHECK(r.start() == CycleRefusal::bad_config);
    }
    {
        GoalBook b;  // ungültig (nie gesetzt)
        RuleGuard guard(guard_config());
        CycleController cc(cycle_config(), b, guard, 0.0);
        CHECK(cc.request_cycle(CycleRequest{"G0", 0, CycleSource::preload, -1.0}, 0, 0, 0.0) ==
              CycleRefusal::bad_config);
    }
    {
        Rig r;
        REQUIRE(r.start() == CycleRefusal::none);
        CHECK(r.start() == CycleRefusal::busy);  // läuft schon; der RoutePlayer ruft vorher cancel
        CHECK(r.cc.state() == CycleState::picking);
    }
}

TEST_CASE("Höhe an der Decke ist erlaubt, darüber nicht") {
    Rig r;
    // G0 auf Ebene 5: 18 + 20 + 3 = 41 > 39; Ebene 4: 18 + 16 + 3 = 37 <= 39.
    CHECK(r.start(CycleSource::preload, "G0", 5) == CycleRefusal::too_high);
    CHECK(r.start(CycleSource::preload, "G0", 4) == CycleRefusal::none);
}

TEST_CASE("Ein neuer Zyklus nach Abschluss löscht das alte Ergebnis") {
    Rig r;
    REQUIRE(r.start() == CycleRefusal::none);
    r.advance_to(CycleState::retreating);
    r.pulse(&CycleInputs::retreat_done);
    REQUIRE(r.cc.result().finished);
    REQUIRE(r.start() == CycleRefusal::none);
    CHECK_FALSE(r.cc.result().finished);
    CHECK_FALSE(r.cc.result().placed);
}
