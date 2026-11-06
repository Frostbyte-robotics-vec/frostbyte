#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "doctest/doctest.h"
#include "sim/game.hpp"
#include "sim/world.hpp"

// Stau der Intake-Rollen im Match-Sim (SIM-31, ADR-037): Game::start_intake lässt das echte IntakeSubsystem gegen die Rollen
// laufen. Alle Zahlen gehören dem Test (nur Wahrscheinlichkeiten 0 und 1), nicht sim/sim_params.json oder config/.
namespace {

using namespace frostbyte;
using namespace frostbyte::sim;

constexpr int end_ms = 60000;
const std::string fixtures = FB_TEST_FIXTURES;

IntakeConfig intake_cfg() {
    IntakeConfig c;
    c.run_v = 10.0;
    c.reverse_v = -8.0;
    c.run_timeout_s = 4.0;
    c.grace_s = 0.3;
    c.stall_deg_s = 20.0;
    c.jam_s = 0.25;
    c.reverse_s = 0.4;
    c.max_retries = 2;
    c.bumper_debounce_s = 0.05;
    return c;
}

GameSetup setup(double jam_prob, double clear_prob) {
    GameSetup g;
    g.goals.push_back(GoalState{Zone::red_quadrant, ToggleColor::red, {}});
    g.goal_names = {"GOAL_RED"};
    g.events.loader = {1.0, 0.0, 0.0};
    g.events.floor = {2.0, 0.0, 0.0};
    g.events.yellow_up_loader = 1.0;
    g.events.yellow_up_floor = 1.0;
    g.events.grasp_success = 1.0;
    g.events.place_success_level0 = 1.0;
    MechanismParams m;
    m.lift = JointParams{0.0, 90.0, 90.0, 0.0};
    m.claw = ClawParams{1.0, 0.2, 4.0};
    g.mech[0] = g.mech[1] = m;
    g.jam = IntakeJamParams{300.0, jam_prob, clear_prob};
    g.intake_cfg[0] = g.intake_cfg[1] = intake_cfg();
    return g;
}

std::vector<std::string> all_events(Game& game, int robot, int from_ms, int to_ms) {
    std::vector<std::string> out;
    for (int t = from_ms; t <= to_ms; ++t) {
        game.step(t);
        for (const std::string& e : game.take_events(robot, t))
            out.push_back(e);
    }
    return out;
}

}  // namespace

TEST_CASE("Stau: ohne Klemmen (Wahrscheinlichkeit 0) bleibt die Aufnahme wie ohne Modell, ohne Ereignis") {
    GameSetup plain = setup(0.0, 0.0);
    plain.intake_cfg[0] = plain.intake_cfg[1] = IntakeConfig{};  // Modell aus
    Game a(plain, 1, 2, end_ms);
    Game b(setup(0.0, 0.0), 1, 2, end_ms);
    const CycleTicket ta = a.start_intake(1000, 0, Source::loader);
    const CycleTicket tb = b.start_intake(1000, 0, Source::loader);
    CHECK(tb.done_ms == ta.done_ms);
    CHECK(tb.ok == ta.ok);
    CHECK(all_events(b, 0, 1000, tb.done_ms).empty());
}

TEST_CASE("Stau: klemmt immer, Impuls löst immer: Verzug = Anlauf + Stauzeit + Impuls, Ereignisse in Folge") {
    Game plain(setup(0.0, 0.0), 1, 2, end_ms);
    Game game(setup(1.0, 1.0), 1, 2, end_ms);
    const CycleTicket base = plain.start_intake(1000, 0, Source::loader);
    const CycleTicket t = game.start_intake(1000, 0, Source::loader);
    REQUIRE(t.accepted);
    CHECK(t.ok);
    // Stau nach grace 0,3 s + jam 0,25 s (+ ein Takt Raster), Impuls 0,4 s: etwa 0,95 s Verzug
    const int delay = t.done_ms - base.done_ms;
    CHECK(delay >= 900);
    CHECK(delay <= 1000);
    const std::vector<std::string> ev = all_events(game, 0, 1000, t.done_ms);
    CHECK(ev == std::vector<std::string>{"INTAKE:JAM", "INTAKE:IMPULSE"});
}

TEST_CASE("Stau: Ereignisse tragen ihre Zeit, nicht die des Aufnahmestarts") {
    Game game(setup(1.0, 1.0), 1, 2, end_ms);
    game.start_intake(1000, 0, Source::loader);
    CHECK(all_events(game, 0, 1000, 1400).empty());  // Stau erst nach etwa 0,55 s
    CHECK(all_events(game, 0, 1401, 1700).size() == 2);
}

TEST_CASE("Stau: Impuls löst nie: Fehler (jam) nach max_retries + 1 Staus, Aufnahme scheitert") {
    Game game(setup(1.0, 0.0), 1, 2, end_ms);
    const CycleTicket t = game.start_intake(1000, 0, Source::loader);
    REQUIRE(t.accepted);
    CHECK_FALSE(t.ok);
    const std::vector<std::string> ev = all_events(game, 0, 1000, t.done_ms);
    const int jams = static_cast<int>(std::count(ev.begin(), ev.end(), "INTAKE:JAM"));
    CHECK(jams == intake_cfg().max_retries + 1);
    CHECK(std::count(ev.begin(), ev.end(), "INTAKE:IMPULSE") == intake_cfg().max_retries);
    REQUIRE_FALSE(ev.empty());
    CHECK(ev.back() == "INTAKE:ERROR_JAM");
    // nichts in der Hand: der Zyklus danach wird abgelehnt
    CHECK_FALSE(game.start_cycle(t.done_ms, 0, 0, game_level_auto, false).accepted);
}

TEST_CASE("Stau: cancel mitten im Stau verwirft die späteren Ereignisse") {
    Game game(setup(1.0, 0.0), 1, 2, end_ms);
    const CycleTicket t = game.start_intake(1000, 0, Source::loader);
    REQUIRE(t.done_ms > 1800);
    all_events(game, 0, 1000, 1700);
    game.cancel(1700, 0);
    CHECK(all_events(game, 0, 1701, t.done_ms + 100).empty());
}

TEST_CASE("Stau: Ereignisse nach Matchende werden nicht geloggt") {
    Game game(setup(1.0, 0.0), 1, 2, 1300);
    game.start_intake(1000, 0, Source::loader);
    CHECK(all_events(game, 0, 1000, 3000).empty());  // der erste Stau käme erst bei etwa 1550 ms
}

TEST_CASE("Stau: das Timeout des Laufs endet vor dem Fehler, wenn max_retries groß ist") {
    GameSetup g = setup(1.0, 0.0);
    g.intake_cfg[0].max_retries = 10;
    g.intake_cfg[0].run_timeout_s = 1.2;
    Game game(g, 1, 2, end_ms);
    const CycleTicket t = game.start_intake(1000, 0, Source::loader);
    CHECK_FALSE(t.ok);
    CHECK(t.done_ms == 1000 + 1200);
    const std::vector<std::string> ev = all_events(game, 0, 1000, t.done_ms);
    REQUIRE_FALSE(ev.empty());
    CHECK(ev.back() == "TIMEOUT:INTAKE");
    CHECK(std::count(ev.begin(), ev.end(), "INTAKE:ERROR_JAM") == 0);
}

TEST_CASE("Stau: das Objektmodell zieht mit und ohne Stau gleich (nur der Verzug kommt dazu)") {
    GameSetup g = setup(0.0, 0.0);
    g.events.loader = {1.0, 0.3, 0.0};  // zufällige Dauer
    GameSetup j = g;
    j.jam.jam_prob = 1.0;
    j.jam.clear_prob = 1.0;
    for (std::uint64_t seed = 1; seed <= 5; ++seed) {
        Game a(g, seed, seed + 1, end_ms);
        Game b(j, seed, seed + 1, end_ms);
        const int delay =
            b.start_intake(1000, 0, Source::loader).done_ms - a.start_intake(1000, 0, Source::loader).done_ms;
        CHECK(delay >= 900);
        CHECK(delay <= 1000);
    }
}

namespace {

SimConfig world_cfg(double jam_prob, double clear_prob) {
    SimConfig cfg = load_config(fixtures + "/sim_params_game.json", fixtures + "/field_game.json",
                                fixtures + "/game_intake_abort.route", fixtures + "/empty.route", "none");
    cfg.game.jam = IntakeJamParams{300.0, jam_prob, clear_prob};
    cfg.game.intake_cfg[0] = cfg.game.intake_cfg[1] = intake_cfg();
    return cfg;
}

std::string run(const SimConfig& cfg, std::uint64_t seed) {
    std::ostringstream s, r;
    run_match(cfg, seed, s, r);
    return s.str() + "|" + r.str();
}

}  // namespace

TEST_CASE("Stau im Routenlauf: Ereignisse stehen im Log; gleicher Seed = byte-gleiches Ergebnis") {
    const SimConfig jam = world_cfg(1.0, 0.0);
    const std::string a = run(jam, 4);
    CHECK(a.find("INTAKE:JAM") != std::string::npos);
    CHECK(a.find("INTAKE:IMPULSE") != std::string::npos);
    CHECK(a.find("INTAKE:ERROR_JAM") != std::string::npos);
    CHECK(a == run(jam, 4));
    const std::string none = run(world_cfg(0.0, 0.0), 4);
    CHECK(none.find("INTAKE:") == std::string::npos);
}

TEST_CASE("Stau: load_game_params liest ereignisse.intake_stau, fehlt der Block, bleibt das Modell aus") {
    // Fixture mit eingesetztem Block: eigene Zahlen, unabhängig von sim/sim_params.json
    std::ifstream in(fixtures + "/sim_params_game.json", std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    std::string with = text.str();
    const std::string key = "\"ereignisse\": {";
    const auto at = with.find(key);
    REQUIRE(at != std::string::npos);
    with.insert(at + key.size(),
                "\"intake_stau\": {\"rollen_deg_s\": 250, \"klemmt\": 0.25, \"loest_nach_impuls\": 0.75},");
    GameSetup g;
    load_game_params(parse_json(with), g);
    CHECK(g.jam.max_deg_s == 250.0);
    CHECK(g.jam.jam_prob == 0.25);
    CHECK(g.jam.clear_prob == 0.75);
    GameSetup h;
    load_game_params(parse_json(text.str()), h);
    CHECK(h.jam.jam_prob == 0.0);
}
