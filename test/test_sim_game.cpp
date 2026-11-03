#include <string>

#include "doctest/doctest.h"
#include "sim/game.hpp"
#include "sim/json.hpp"
#include "sim/scorer.hpp"

using namespace frostbyte::sim;

namespace {

constexpr int end_ms = 60000;

// Alle Zahlen hier gehören dem Test, nicht sim_params.json: Erfolg überall 1, Dauer ohne Streuung.
EventParams certain_events() {
    EventParams e;
    e.loader = {1.0, 0.0, 0.0};
    e.floor = {1.0, 0.0, 0.0};
    e.yellow_up_loader = 1.0;
    e.yellow_up_floor = 1.0;
    e.grasp_success = 1.0;
    e.place_success_level0 = 1.0;
    e.place_penalty_per_level = 0.0;
    return e;
}

MechanismParams test_mech() {
    MechanismParams m;
    m.lift = JointParams{0.0, 90.0, 90.0, 0.0};
    m.claw = ClawParams{1.0, 0.2, 4.0};
    return m;
}

GameSetup one_goal(Zone zone, ToggleColor toggle) {
    GameSetup g;
    g.goals.push_back(GoalState{zone, toggle, {}});
    g.events = certain_events();
    g.mech[0] = g.mech[1] = test_mech();
    return g;
}

void run_to_end(Game& game) {
    for (int t = 0; t <= end_ms; ++t)
        game.step(t);
}

}  // namespace

TEST_CASE("Game: ohne Ziele ist es aus und liefert keinen Punktestand") {
    GameSetup g;
    CHECK_FALSE(g.enabled());
}

TEST_CASE("Game: drei sichere Zyklen bauen Pin, Cup, Pin und der Punktestand ist der des Scorers") {
    GameSetup g = one_goal(Zone::red_quadrant, ToggleColor::red);
    g.cycles = {{1000, 0, 0, 0, Source::loader}, {11000, 0, 0, 0, Source::loader}, {21000, 0, 0, 0, Source::loader}};
    Game game(g, 1, 2, end_ms);
    CHECK(game.score() == 0);
    run_to_end(game);
    CHECK(game.completed_cycles() == 3);
    CHECK(game.failed_cycles() == 0);
    CHECK(game.skipped_cycles() == 0);

    ScoreInput in;
    in.goals.push_back(GoalState{Zone::red_quadrant,
                                 ToggleColor::red,
                                 {StackItem::make_pin(PinColor::red, PinColor::yellow), StackItem::make_cup(false),
                                  StackItem::make_pin(PinColor::red, PinColor::yellow)}});
    CHECK(game.score() == score_skills(in).total);
    CHECK(game.score() > 0);
}

TEST_CASE("Game: der Punktestand wächst erst, wenn der Zyklus fertig ist, nicht beim Start") {
    GameSetup g = one_goal(Zone::red_quadrant, ToggleColor::red);
    g.cycles = {{1000, 0, 0, 0, Source::loader}};
    Game game(g, 1, 2, end_ms);
    int first_nonzero_ms = -1;
    for (int t = 0; t <= end_ms; ++t) {
        game.step(t);
        if (first_nonzero_ms < 0 && game.score() > 0)
            first_nonzero_ms = t;
    }
    // Aufnahme 1,0 s + Greifer + Lift: mehr als 1 s nach dem Start.
    CHECK(first_nonzero_ms > 2000);
}

TEST_CASE("Game: Fehlschläge setzen nichts ab und werden gezählt") {
    GameSetup g = one_goal(Zone::red_quadrant, ToggleColor::red);
    g.events.place_success_level0 = 0.0;
    g.cycles = {{1000, 0, 0, 0, Source::loader}, {11000, 0, 0, 0, Source::loader}};
    Game game(g, 1, 2, end_ms);
    run_to_end(game);
    CHECK(game.score() == 0);
    CHECK(game.failed_cycles() == 2);
    CHECK(game.completed_cycles() == 0);
}

TEST_CASE("Game: ein Zyklus, der beginnt, während der Roboter noch einen fährt, wird übersprungen und gezählt") {
    GameSetup g = one_goal(Zone::red_quadrant, ToggleColor::red);
    g.cycles = {{1000, 0, 0, 0, Source::loader}, {1100, 0, 0, 0, Source::loader}};
    Game game(g, 1, 2, end_ms);
    run_to_end(game);
    CHECK(game.completed_cycles() == 1);
    CHECK(game.skipped_cycles() == 1);
}

TEST_CASE("Game: zwei Roboter bauen unabhängig, Zyklus nach Matchende zählt nicht") {
    GameSetup g = one_goal(Zone::blue_quadrant, ToggleColor::blue);
    g.cycles = {{1000, 0, 0, 0, Source::loader}, {1000, 1, 0, 0, Source::loader}, {59500, 0, 0, 0, Source::loader}};
    Game game(g, 1, 2, end_ms);
    run_to_end(game);
    CHECK(game.completed_cycles() == 2);  // Zyklus ab 59,5 s endet nach 0:00
    ScoreInput in;
    in.goals.push_back(GoalState{Zone::blue_quadrant,
                                 ToggleColor::blue,
                                 {StackItem::make_pin(PinColor::blue, PinColor::yellow), StackItem::make_cup(false)}});
    CHECK(game.score() == score_skills(in).total);
}

TEST_CASE("Game: gleiche Seeds = gleicher Verlauf, mit zufälligen Fehlschlägen") {
    const auto run = [](std::uint64_t seed) {
        GameSetup g = one_goal(Zone::red_quadrant, ToggleColor::red);
        g.events.place_success_level0 = 0.5;
        for (int k = 0; k < 5; ++k)
            g.cycles.push_back({1000 + 10000 * k, 0, 0, 0, Source::loader});
        Game game(g, seed, seed + 1, end_ms);
        run_to_end(game);
        return std::to_string(game.score()) + "/" + std::to_string(game.failed_cycles());
    };
    CHECK(run(5) == run(5));
}

TEST_CASE("Midfield: Quadrat ist um 45 Grad gedreht, Berühren eines Teils genügt, höchstens zwei zählen") {
    MidfieldSpec m;
    m.present = true;
    m.side_in = 48.0;  // Halbdiagonale 33,94
    const auto box = [](double x, double y) { return Box{x, y, 24.0, 24.0, 0.0}; };
    CHECK(robots_in_midfield(m, {box(0, 0)}) == 1);
    CHECK(robots_in_midfield(m, {box(40, 0)}) == 1);  // die Spitze des Rhombus ragt unter die Kante
    CHECK(robots_in_midfield(m, {box(50, 0)}) == 0);
    CHECK(robots_in_midfield(m, {box(30, 30)}) == 0);  // läge im ungedrehten Quadrat (Ecke 18/18), nicht im gedrehten
    CHECK(robots_in_midfield(m, {box(0, 0), box(0, 10), box(5, 5)}) == 2);
    m.present = false;
    CHECK(robots_in_midfield(m, {box(0, 0)}) == 0);
}

TEST_CASE("Midfield: field.json-Block wird gelesen, ohne Block ist es nicht vorhanden") {
    const MidfieldSpec m =
        load_midfield(parse_json(R"({"midfield": {"seite_in": 48, "mitte_x_in": 1, "mitte_y_in": -2}})"));
    CHECK(m.present);
    CHECK(m.side_in == 48.0);
    CHECK(m.x_in == 1.0);
    CHECK(m.y_in == -2.0);
    CHECK_FALSE(load_midfield(parse_json(R"({"feld_in": 144})")).present);
}

TEST_CASE("Game: Punktestand mit Robotern im Midfield, gelb im Midfield-Goal braucht einen Roboter dort") {
    GameSetup g = one_goal(Zone::midfield, ToggleColor::neutral);
    g.cycles = {{1000, 0, 0, 0, Source::loader}};
    Game game(g, 1, 2, end_ms);
    run_to_end(game);
    CHECK(game.score(0) == 0);   // Pin(rot, gelb): unten im Goal, oben gelb, aber nicht Owned
    CHECK(game.score(1) == 18);  // gelb 10 + 8 für den Roboter
    CHECK(game.score(2) == 26);
    CHECK(game.score(7) == 26);  // begrenzt auf zwei Roboter
}

TEST_CASE("Game: Besitz von Zyklusstart bis Zyklusende, erst Pin, dann Cup, der andere Roboter bleibt leer") {
    GameSetup g = one_goal(Zone::red_quadrant, ToggleColor::red);
    g.cycles = {{1000, 0, 0, 0, Source::loader}, {11000, 0, 0, 0, Source::loader}};
    Game game(g, 1, 2, end_ms);
    for (int t = 0; t <= 500; ++t)
        game.step(t);
    CHECK(game.pins(0) == 0);
    for (int t = 501; t <= 1000; ++t)
        game.step(t);
    CHECK(game.pins(0) == 1);  // erster Zyklus: Pin
    CHECK(game.cups(0) == 0);
    CHECK(game.pins(1) == 0);
    for (int t = 1001; t <= 10000; ++t)
        game.step(t);
    CHECK(game.pins(0) == 0);  // abgelegt
    for (int t = 10001; t <= 11000; ++t)
        game.step(t);
    CHECK(game.cups(0) == 1);  // zweiter Zyklus: Cup auf den Pin
    CHECK(game.pins(0) == 0);
    for (int t = 11001; t <= 20000; ++t)
        game.step(t);
    CHECK(game.cups(0) == 0);
}
