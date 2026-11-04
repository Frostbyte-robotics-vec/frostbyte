#include <cmath>
#include <sstream>
#include <string>
#include <vector>

#include "doctest/doctest.h"
#include "sim/game.hpp"
#include "sim/world.hpp"

// Zyklen aus Routen (SIM-22, ADR-032): Game::start_intake/start_cycle/cancel und der Anschluss im Routenlauf.
// Alle Zahlen gehören dem Test (Fixtures game_*.json), nicht sim/sim_params.json.
namespace {

using namespace frostbyte::sim;

constexpr int end_ms = 60000;

EventParams certain_events() {
    EventParams e;
    e.loader = {1.0, 0.0, 0.0};
    e.floor = {2.0, 0.0, 0.0};
    e.yellow_up_loader = 1.0;
    e.yellow_up_floor = 1.0;
    e.grasp_success = 1.0;
    e.place_success_level0 = 1.0;
    e.place_penalty_per_level = 0.0;
    return e;
}

GameSetup one_goal() {
    GameSetup g;
    g.goals.push_back(GoalState{Zone::red_quadrant, ToggleColor::red, {}});
    g.goal_names = {"GOAL_RED"};
    g.events = certain_events();
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

}  // namespace

TEST_CASE("Game: ein Routenzyklus erscheint erst nach seiner Dauer im Stapel und im Punktestand") {
    Game game(one_goal(), 1, 2, end_ms);
    const CycleTicket t = game.start_cycle(1000, 0, 0, game_level_auto, true);
    REQUIRE(t.accepted);
    CHECK(t.ok);
    CHECK(t.done_ms > 1000);
    advance(game, 1000, t.done_ms - 1);
    CHECK(game.score() == 0);
    CHECK(game.completed_cycles() == 0);
    advance(game, t.done_ms, t.done_ms);
    CHECK(game.completed_cycles() == 1);
    CHECK(game.score() > 0);
}

TEST_CASE("Game: Besitz gilt vom Start des Routenzyklus bis zu seinem Ende") {
    Game game(one_goal(), 1, 2, end_ms);
    const CycleTicket t = game.start_cycle(1000, 0, 0, game_level_auto, true);
    advance(game, 1000, t.done_ms - 1);
    CHECK(game.pins(0) == 1);
    CHECK(game.pins(1) == 0);
    advance(game, t.done_ms, t.done_ms);
    CHECK(game.pins(0) == 0);
}

TEST_CASE("Game: level=auto ist die Zahl der Teile auf dem Goal, Pin und Cup wechseln sich ab") {
    Game game(one_goal(), 1, 2, end_ms);
    CycleTicket c = game.start_cycle(1000, 0, 0, game_level_auto, true);  // Preload
    REQUIRE(c.accepted);
    advance(game, 1000, c.done_ms);
    for (int i = 0; i < 2; ++i) {
        const CycleTicket in = game.start_intake(c.done_ms + 10, 0, Source::loader);
        REQUIRE(in.accepted);
        advance(game, c.done_ms + 11, in.done_ms);
        const int from = in.done_ms;
        c = game.start_cycle(from, 0, 0, game_level_auto, false);
        REQUIRE(c.accepted);
        advance(game, from + 1, c.done_ms);
    }
    CHECK(game.completed_cycles() == 3);
    ScoreInput in;
    in.goals.push_back(GoalState{Zone::red_quadrant,
                                 ToggleColor::red,
                                 {StackItem::make_pin(PinColor::red, PinColor::yellow), StackItem::make_cup(false),
                                  StackItem::make_pin(PinColor::red, PinColor::yellow)}});
    CHECK(game.score() == score_skills(in).total);
}

TEST_CASE("Game: ein ausdrückliches level, das nicht zur Stapelhöhe passt, wird abgelehnt und verbraucht nichts") {
    Game game(one_goal(), 1, 2, end_ms);
    CHECK_FALSE(game.start_cycle(1000, 0, 0, 2, true).accepted);  // auf dem Goal liegt nichts: Ebene 0
    CHECK(game.rejected_cycles() == 1);
    const CycleTicket ok = game.start_cycle(1000, 0, 0, 0, true);  // der Preload ist noch da
    CHECK(ok.accepted);
}

TEST_CASE("Game: über Ebene 5 hinaus wird abgelehnt") {
    GameSetup g = one_goal();
    for (int i = 0; i <= game_max_level; ++i)
        g.goals[0].stack.push_back(i % 2 == 0 ? StackItem::make_pin(PinColor::red, PinColor::red)
                                              : StackItem::make_cup(false));
    Game game(g, 1, 2, end_ms);  // sechs Teile liegen schon: das nächste wäre Ebene 6
    CHECK_FALSE(game.start_cycle(1000, 0, 0, game_level_auto, true).accepted);
    CHECK_FALSE(game.start_cycle(1000, 0, 0, game_max_level + 1, true).accepted);
}

TEST_CASE("Game: ohne Einheit in der Hand gibt es keinen Zyklus (Preload nur einmal, quiver ohne Modell)") {
    Game game(one_goal(), 1, 2, end_ms);
    const CycleTicket a = game.start_cycle(1000, 0, 0, game_level_auto, true);
    REQUIRE(a.accepted);
    advance(game, 1000, a.done_ms);
    CHECK_FALSE(game.start_cycle(a.done_ms + 10, 0, 0, game_level_auto, true).accepted);  // Preload verbraucht
    CHECK(game.rejected_cycles() == 1);
}

TEST_CASE("Game: intake dann cycle: der Zyklus nimmt nicht noch einmal auf") {
    Game with_intake(one_goal(), 1, 2, end_ms);
    const CycleTicket in = with_intake.start_intake(1000, 0, Source::loader);
    REQUIRE(in.accepted);
    CHECK(in.ok);
    CHECK(in.done_ms == 1000 + 1000);  // Loader-Einheit: 1,0 s im Test
    const CycleTicket c = with_intake.start_cycle(in.done_ms, 0, 0, game_level_auto, false);
    REQUIRE(c.accepted);

    Game preload(one_goal(), 1, 2, end_ms);
    const CycleTicket p = preload.start_cycle(in.done_ms, 0, 0, game_level_auto, true);
    CHECK(c.done_ms == p.done_ms);  // beide ohne Aufnahme
}

TEST_CASE("Game: Aufnahme vom Boden dauert länger als vom Loader") {
    Game game(one_goal(), 1, 2, end_ms);
    CHECK(game.start_intake(0, 0, Source::floor).done_ms == 2000);
}

TEST_CASE("Game: ein Zyklus, während der Roboter noch aufnimmt, wird abgelehnt") {
    Game game(one_goal(), 1, 2, end_ms);
    REQUIRE(game.start_intake(1000, 0, Source::loader).accepted);
    CHECK_FALSE(game.start_cycle(1500, 0, 0, game_level_auto, true).accepted);
    CHECK_FALSE(game.start_intake(1500, 0, Source::loader).accepted);
}

TEST_CASE("Game: Fehlschlag der Aufnahme meldet ok = false und hält nichts") {
    GameSetup g = one_goal();
    g.events.loader.fail = 1.0;
    Game game(g, 1, 2, end_ms);
    const CycleTicket in = game.start_intake(1000, 0, Source::loader);
    REQUIRE(in.accepted);
    CHECK_FALSE(in.ok);
    CHECK_FALSE(game.start_cycle(in.done_ms, 0, 0, game_level_auto, false).accepted);
}

TEST_CASE("Game: Fehlschlag beim Setzen meldet ok = false, Stapel und Punkte bleiben leer") {
    GameSetup g = one_goal();
    g.events.place_success_level0 = 0.0;
    Game game(g, 1, 2, end_ms);
    const CycleTicket c = game.start_cycle(1000, 0, 0, game_level_auto, true);
    REQUIRE(c.accepted);
    CHECK_FALSE(c.ok);
    advance(game, 1000, end_ms);
    CHECK(game.score() == 0);
    CHECK(game.failed_cycles() == 1);
}

TEST_CASE("Game: cancel vor dem Ende zieht den Zyklus zurück") {
    Game game(one_goal(), 1, 2, end_ms);
    const CycleTicket c = game.start_cycle(1000, 0, 0, game_level_auto, true);
    REQUIRE(c.accepted);
    REQUIRE(c.done_ms > 1100);  // Preload: Greifer 0,2 s
    advance(game, 1000, 1100);
    game.cancel(1100, 0);
    advance(game, 1101, end_ms);
    CHECK(game.score() == 0);
    CHECK(game.completed_cycles() == 0);
    CHECK(game.pins(0) == 0);
    // Der Roboter ist wieder frei (die Einheit ist allerdings verloren).
    CHECK(game.start_intake(1150, 0, Source::loader).accepted);
}

TEST_CASE("Game: cancel nach dem Ende ändert nichts") {
    Game game(one_goal(), 1, 2, end_ms);
    const CycleTicket c = game.start_cycle(1000, 0, 0, game_level_auto, true);
    advance(game, 1000, c.done_ms);
    game.cancel(c.done_ms + 5, 0);
    CHECK(game.completed_cycles() == 1);
    CHECK(game.score() > 0);
}

TEST_CASE("Game: cancel während der Aufnahme verwirft die Einheit") {
    Game game(one_goal(), 1, 2, end_ms);
    const CycleTicket in = game.start_intake(1000, 0, Source::loader);
    game.cancel(1200, 0);
    CHECK_FALSE(game.start_cycle(in.done_ms + 10, 0, 0, game_level_auto, false).accepted);
}

TEST_CASE("Game: goal_index löst Namen auf, unbekannte ergeben -1") {
    Game game(one_goal(), 1, 2, end_ms);
    CHECK(game.goal_index("GOAL_RED") == 0);
    CHECK(game.goal_index("NOPE") == -1);
}

// --- Routenläufe -----------------------------------------------------------------------------------------------

namespace {

const std::string fixtures = FB_TEST_FIXTURES;

RouteControl test_control() {
    RouteControl c;
    c.motion.max_volts = 11.5;
    c.motion.track_width_in = 12.0;
    c.motion.follow_max_volts = 8.0;
    c.lateral.kp = 1.0;
    c.lateral.kd = 0.08;
    c.lateral.out_max_v = 12.0;
    c.angular.kp = 0.15;
    c.angular.ki = 0.5;
    c.angular.kd = 0.01;
    c.angular.i_zone = 10.0;
    c.angular.i_max_v = 2.0;
    c.angular.out_max_v = 12.0;
    return c;
}

SimConfig load(const std::string& stacker, const std::string& runner, const char* params = "sim_params_game.json") {
    SimConfig cfg = load_config(fixtures + "/" + params, fixtures + "/field_game.json", fixtures + "/" + stacker,
                                fixtures + "/" + runner, "none");
    cfg.stacker.control = test_control();
    cfg.runner.control = test_control();
    return cfg;
}

struct Rows {
    std::vector<std::vector<std::string>> r;
    int col(const std::string& name) const {
        for (std::size_t i = 0; i < head.size(); ++i)
            if (head[i] == name)
                return static_cast<int>(i);
        return -1;
    }
    std::vector<std::string> head;
};

Rows parse(const std::string& csv) {
    Rows out;
    std::stringstream in(csv);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::vector<std::string> cells;
        std::stringstream ls(line);
        for (std::string cell; std::getline(ls, cell, ',');)
            cells.push_back(cell);
        if (out.head.empty())
            out.head = cells;
        else
            out.r.push_back(cells);
    }
    return out;
}

struct Run {
    std::string stacker, runner;
};

Run run(const SimConfig& cfg, std::uint64_t seed = 1) {
    std::ostringstream s, r;
    run_match(cfg, seed, s, r);
    return {s.str(), r.str()};
}

// Erste Zeit (s), zu der score > 0 ist; -1, wenn nie.
double first_score_s(const std::string& csv) {
    const Rows t = parse(csv);
    const int c = t.col("score");
    for (const auto& row : t.r)
        if (c >= 0 && row.size() > static_cast<std::size_t>(c) && !row[c].empty() && std::stoi(row[c]) > 0)
            return std::stod(row[0]);
    return -1.0;
}

int max_score(const std::string& csv) {
    const Rows t = parse(csv);
    const int c = t.col("score");
    int m = 0;
    for (const auto& row : t.r)
        if (row.size() > static_cast<std::size_t>(c) && !row[c].empty())
            m = std::max(m, std::stoi(row[c]));
    return m;
}

bool has(const std::string& text, const std::string& what) { return text.find(what) != std::string::npos; }

}  // namespace

TEST_CASE("Routenlauf: der cycle der Route landet nach seiner Dauer im Punktestand beider Logs") {
    const Run r = run(load("game_cycle.route", "empty.route"));
    const double s = first_score_s(r.stacker);
    const double q = first_score_s(r.runner);
    CHECK(s > 0.1);  // Preload: Greifer schließt erst (0,2 s im Test), nicht sofort
    CHECK(s == q);
    CHECK(max_score(r.stacker) > 0);
    CHECK(max_score(r.stacker) == max_score(r.runner));
}

TEST_CASE("Routenlauf: Besitz während des Zyklus, danach nicht mehr") {
    const Run r = run(load("game_cycle.route", "empty.route"));
    const Rows t = parse(r.stacker);
    const int pin = t.col("has_pin");
    bool held = false, released = false;
    for (const auto& row : t.r) {
        if (row[pin] == "1")
            held = true;
        else if (held)
            released = true;
    }
    CHECK(held);
    CHECK(released);
}

TEST_CASE("Routenlauf: intake und weitere cycle: drei Teile im Stapel, mehr Punkte als nach einem") {
    const Run one = run(load("game_cycle.route", "empty.route"));
    const Run three = run(load("game_three_cycles.route", "empty.route"));
    CHECK(max_score(three.stacker) > max_score(one.stacker));
    CHECK(first_score_s(three.stacker) == first_score_s(one.stacker));  // der erste Zyklus ist derselbe Preload
}

TEST_CASE("Routenlauf: der Zyklus des Runners zählt in den Punktestand des Teams (beide Logs gleich)") {
    const Run r = run(load("empty.route", "game_runner.route"));
    CHECK(max_score(r.runner) > 0);
    CHECK(max_score(r.stacker) == max_score(r.runner));
}

TEST_CASE("Routenlauf: Timeout kürzer als der Zyklus: nichts landet im Stapel") {
    const Run r = run(load("game_short_timeout.route", "empty.route"));
    CHECK(max_score(r.stacker) == 0);
}

TEST_CASE("Routenlauf: fehlgeschlagene Aufnahme meldet failed, on_timeout=abort beendet die Route") {
    const Run r = run(load("game_intake_abort.route", "empty.route", "sim_params_game_pickfail.json"));
    CHECK(has(r.stacker, "2:intake"));
    // route_step zeigt bei skip und abort dasselbe; der Unterschied ist, dass der Preload-cycle danach nicht mehr läuft.
    CHECK(max_score(r.stacker) == 0);
}

TEST_CASE("Routenlauf: bei skip läuft die Route nach fehlgeschlagener Aufnahme weiter (der Preload wird gesetzt)") {
    const Run r = run(load("game_fail_skip.route", "empty.route", "sim_params_game_pickfail.json"));
    CHECK(max_score(r.stacker) > 0);
}

TEST_CASE("Routenlauf: ein level, das nicht zur Stapelhöhe passt, scheitert, es kommt kein Punkt dazu") {
    const Run r = run(load("game_level_wrong.route", "empty.route"));
    CHECK(max_score(r.stacker) == 0);
}

TEST_CASE("Routenlauf: gleicher Seed = byte-gleiche Logs mit Spielzustand und Zufall, anderer Seed nicht") {
    SimConfig cfg = load_config(fixtures + "/sim_params_game_random.json", fixtures + "/field_game.json",
                                fixtures + "/game_three_cycles.route", fixtures + "/game_runner.route", "realistic");
    cfg.stacker.control = test_control();
    cfg.runner.control = test_control();
    const Run a = run(cfg, 3), b = run(cfg, 3), c = run(cfg, 4);
    CHECK(a.stacker == b.stacker);
    CHECK(a.runner == b.runner);
    CHECK(a.stacker != c.stacker);
}

// Wahre Endpose (tx, ty) der letzten Zeile.
static std::pair<double, double> final_truth(const std::string& csv) {
    const Rows t = parse(csv);
    const auto& row = t.r.back();
    return {std::stod(row[t.col("tx_in")]), std::stod(row[t.col("ty_in")])};
}

TEST_CASE("Routenlauf: fehlgeschlagener cycle meldet failed: abort hält die Route an, skip fährt zum nächsten Ziel") {
    const Run abort = run(load("game_place_abort.route", "empty.route", "sim_params_game_placefail.json"));
    const Run skip = run(load("game_place_skip.route", "empty.route", "sim_params_game_placefail.json"));
    CHECK(max_score(abort.stacker) == 0);
    CHECK(max_score(skip.stacker) == 0);
    const auto start = final_truth(abort.stacker);
    CHECK(std::hypot(start.first + 20.0, start.second) <= 1.5);  // START_STACKER (-20, 0): nicht gefahren
    const auto goal = final_truth(skip.stacker);
    CHECK(std::hypot(goal.first + 20.0, goal.second - 20.0) <= 1.5);  // Ziel (-20, 20) erreicht
}

TEST_CASE("Routenlauf: ohne cycle/intake in den Routen gibt es keinen Spielzustand, Spalte score bleibt leer") {
    const SimConfig cfg = load("empty.route", "empty.route");
    CHECK_FALSE(cfg.game.enabled());
}

TEST_CASE("Routenlauf: level=auto + Goal aus field.json: Zone folgt dem Quadranten") {
    const SimConfig cfg = load("game_cycle.route", "empty.route");
    REQUIRE(cfg.game.enabled());
    REQUIRE(cfg.game.goals.size() == 2);
    CHECK(cfg.game.goals[0].zone == Zone::red_quadrant);
    CHECK(cfg.game.goals[1].zone == Zone::blue_quadrant);
    CHECK(cfg.game.goal_names[0] == "GOAL_RED");
}

// --- toggle (SIM-23) --------------------------------------------------------------------------------------------

TEST_CASE("Routenlauf: toggle setzt den Toggle des Quadranten, danach zählt das gelbe Pin-Hälfte (Start neutral)") {
    const Run without = run(load("empty.route", "game_runner.route", "sim_params_game_toggle.json"));
    const Run with = run(load("empty.route", "game_toggle.route", "sim_params_game_toggle.json"));
    CHECK(max_score(without.runner) == 0);  // gelb bei neutralem Toggle: nicht Owned (SC4, RSC3a.iii.1)
    CHECK(max_score(with.runner) == 10);
    CHECK(max_score(with.stacker) == 10);  // der Teamstand steht in beiden Logs
}

TEST_CASE("Routenlauf: ein toggle mit Timeout kürzer als die Dauer setzt nichts") {
    const Run r = run(load("empty.route", "game_toggle_short.route", "sim_params_game_toggle.json"));  // 1 s < 2 s
    CHECK(max_score(r.runner) == 0);
}

TEST_CASE("Routenlauf: der Spielzustand liest toggles aus field.json (Name, Zielfarbe, Goals des Quadranten)") {
    const SimConfig cfg = load("empty.route", "game_toggle.route", "sim_params_game_toggle.json");
    REQUIRE(cfg.game.toggles.size() == 1);
    CHECK(cfg.game.toggles[0].name == "TOGGLE_T");
    CHECK(cfg.game.toggles[0].color == ToggleColor::blue);
    CHECK(cfg.game.toggles[0].goals == std::vector<int>{1});  // GOAL_BLUE im Quadranten BLUE_1
    CHECK(cfg.game.toggle_ms == 2000);
}

TEST_CASE("Routenlauf: toggle weiter vom Toggle als die Reichweite setzt nichts (Fixture mit kleiner Reichweite)") {
    // Toggle bei (30, 30), Runner startet bei (20, 0): Abstand 31,6 in; Reichweite in der Fixture 10 in.
    const Run r = run(load("empty.route", "game_toggle.route", "sim_params_game_toggle_far.json"));
    CHECK(max_score(r.runner) == 0);
}

TEST_CASE("Routenlauf: der Spielzustand liest Toggle-Position und Reichweite") {
    const SimConfig cfg = load("empty.route", "game_toggle.route", "sim_params_game_toggle.json");
    REQUIRE(cfg.game.toggles.size() == 1);
    CHECK(cfg.game.toggles[0].x_in == doctest::Approx(30.0));
    CHECK(cfg.game.toggles[0].y_in == doctest::Approx(30.0));
    CHECK(cfg.game.toggle_reach_in == doctest::Approx(40.0));
}
