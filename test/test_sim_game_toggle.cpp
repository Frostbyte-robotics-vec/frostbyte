#include <string>
#include <vector>

#include "doctest/doctest.h"
#include "sim/game.hpp"

// Toggle im Spielzustand (SIM-23): Game::start_toggle setzt den Toggle eines Quadranten nach toggle_ms auf seine Farbe.
// Alle Zahlen gehören dem Test, nicht sim/sim_params.json.
namespace {

using namespace frostbyte::sim;

constexpr int end_ms = 60000;
constexpr int toggle_ms = 2500;

GameSetup one_goal_neutral_toggle() {
    GameSetup g;
    g.goals.push_back(GoalState{Zone::red_quadrant, ToggleColor::neutral, {}});
    g.goal_names = {"GOAL_RED"};
    g.toggles.push_back(ToggleSpec{"TOGGLE_RED", ToggleColor::red, {0}});
    g.toggle_ms = toggle_ms;
    g.events.loader = {1.0, 0.0, 0.0};
    g.events.floor = {2.0, 0.0, 0.0};
    g.events.yellow_up_loader = 1.0;  // der Pin liegt mit gelber Hälfte oben im Goal
    g.events.yellow_up_floor = 1.0;
    g.events.grasp_success = 1.0;
    g.events.place_success_level0 = 1.0;
    g.events.place_penalty_per_level = 0.0;
    MechanismParams m;
    m.lift = JointParams{0.0, 90.0, 90.0, 0.0};
    m.claw = ClawParams{1.0, 0.2, 4.0};
    g.mech[0] = g.mech[1] = m;
    return g;
}

void advance(Game& game, int from_ms, int to_ms) {
    for (int t = from_ms; t <= to_ms; ++t)
        game.step(t);
}

// Ein gelber Pin im Goal: Aufnahme, dann Zyklus. Rückgabe: Zeit, ab der der Roboter wieder frei ist.
int place_yellow_pin(Game& game, int robot) {
    const CycleTicket in = game.start_intake(0, robot, Source::loader);
    REQUIRE(in.accepted);
    advance(game, 0, in.done_ms);
    const CycleTicket cyc = game.start_cycle(in.done_ms, robot, 0, game_level_auto, false);
    REQUIRE(cyc.accepted);
    advance(game, in.done_ms, cyc.done_ms);
    return cyc.done_ms;
}

}  // namespace

TEST_CASE("Game: ohne gesetzten Toggle zählt das gelbe Pin-Hälfte nicht, nach start_toggle erst nach toggle_ms") {
    Game game(one_goal_neutral_toggle(), 1, 2, end_ms);
    const int free_ms = place_yellow_pin(game, 1);
    CHECK(game.score() == 0);  // gelb, Toggle neutral (SC4): nicht Owned
    const CycleTicket t = game.start_toggle(free_ms, 1, game.toggle_index("TOGGLE_RED"));
    REQUIRE(t.accepted);
    CHECK(t.ok);
    CHECK(t.done_ms == free_ms + toggle_ms);
    advance(game, free_ms, t.done_ms - 1);
    CHECK(game.score() == 0);
    advance(game, t.done_ms, t.done_ms);
    CHECK(game.score() == 10);  // gelb Owned (RSC3a.iii.1)
}

TEST_CASE("Game: start_toggle lehnt unbekannten Toggle und beschäftigten Roboter ab, ohne etwas zu ändern") {
    Game game(one_goal_neutral_toggle(), 1, 2, end_ms);
    CHECK(game.toggle_index("TOGGLE_X") == -1);
    CHECK_FALSE(game.start_toggle(0, 1, -1).accepted);
    const CycleTicket first = game.start_toggle(0, 1, 0);
    REQUIRE(first.accepted);
    CHECK_FALSE(game.start_toggle(100, 1, 0).accepted);  // derselbe Roboter ist noch beim Drehen
    CHECK(game.rejected_cycles() == 2);
    CHECK(game.start_toggle(100, 0, 0).accepted);  // ein anderer Roboter ist frei
}

TEST_CASE("Game: cancel bricht einen laufenden Toggle ab, nach 0:00 zählt keiner mehr") {
    {
        Game game(one_goal_neutral_toggle(), 1, 2, end_ms);
        const int free_ms = place_yellow_pin(game, 1);
        const CycleTicket t = game.start_toggle(free_ms, 1, 0);
        game.cancel(free_ms + 1000, 1);
        advance(game, free_ms, t.done_ms + 10);
        CHECK(game.score() == 0);
    }
    {
        Game game(one_goal_neutral_toggle(), 1, 2, end_ms);
        const int free_ms = place_yellow_pin(game, 1);
        const CycleTicket t = game.start_toggle(end_ms - 1000, 1, 0);  // fertig erst nach 0:00
        REQUIRE(t.accepted);
        advance(game, free_ms, end_ms);
        CHECK(game.score() == 0);
    }
}
