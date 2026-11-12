#include <sstream>
#include <string>

#include "doctest/doctest.h"
#include "sim/game.hpp"
#include "sim/json.hpp"
#include "sim/mechanism.hpp"
#include "sim/world.hpp"

// SG3 in der Simulation (SIM-32): Höhe des Roboters aus dem Lift-Winkel, RuleGuard in jedem Takt des Matches.
// Alle Zahlen gehören dem Test (eigene Tabellen), nichts hängt von den Platzhaltern in sim_params.json ab.
namespace {

using namespace frostbyte::sim;

const std::string fixtures = FB_TEST_FIXTURES;

int count(const std::string& text, const std::string& what) {
    int n = 0;
    for (std::size_t p = text.find(what); p != std::string::npos; p = text.find(what, p + 1))
        ++n;
    return n;
}

HeightMap table(double h0, double h4) {
    HeightMap h;
    REQUIRE(h.set({0.0, 25.0, 50.0, 75.0, 100.0},
                  {h0, h0 + (h4 - h0) * 0.25, h0 + (h4 - h0) * 0.5, h0 + (h4 - h0) * 0.75, h4}));
    return h;
}

SimConfig match_cfg(double top_height_in) {
    SimConfig cfg = load_config(fixtures + "/sim_params_min.json", fixtures + "/field_min.json", "", "", "none");
    GameSetup& g = cfg.game;
    g.events.loader = {1.0, 0.0, 0.0};
    g.events.yellow_up_loader = 1.0;
    g.events.grasp_success = 1.0;
    g.events.place_success_level0 = 1.0;
    g.events.place_penalty_per_level = 0.0;
    for (MechanismParams& m : g.mech) {
        m.lift = {0.0, 100.0, 200.0, 0.0};
        m.claw = {4.0, 2.0, 20.0};
    }
    g.mech[0].height = table(10.0, top_height_in);  // nur der Stacker hat eine Höhentabelle
    g.goals.push_back(GoalState{Zone::red_quadrant, ToggleColor::red, {}});
    g.cycles.push_back({5000, 0, 0, game_max_level, Source::loader});  // Hub ganz nach oben und zurück
    return cfg;
}

}  // namespace

TEST_CASE("HeightMap: Stützpunkte genau, dazwischen steigend, außerhalb begrenzt") {
    const HeightMap h = table(10.0, 50.0);
    CHECK(h.at(0.0) == doctest::Approx(10.0));
    CHECK(h.at(50.0) == doctest::Approx(30.0));
    CHECK(h.at(100.0) == doctest::Approx(50.0));
    CHECK(h.at(60.0) > h.at(50.0));
    CHECK(h.at(60.0) < h.at(75.0));
    CHECK(h.at(-30.0) == doctest::Approx(10.0));
    CHECK(h.at(500.0) == doctest::Approx(50.0));
}

TEST_CASE("HeightMap: ungültige Tabelle (nicht steigend) wird nicht gültig") {
    HeightMap h;
    CHECK_FALSE(h.set({0.0, 25.0, 25.0, 75.0, 100.0}, {10.0, 20.0, 30.0, 40.0, 50.0}));
    CHECK_FALSE(h.valid());
}

TEST_CASE("Mechanismen laden: hoehe aus eingebettetem JSON (nur Ladecode), ohne hoehe nicht modelliert") {
    const char* text = R"({"roboter":{"a":{"mechanismen":{
        "lift":{"min_deg":0,"max_deg":100,"max_deg_s":90,"max_deg_s2":500,
                "hoehe":{"winkel_deg":[0,25,50,75,100],"hoehe_in":[9,19,29,39,49],"quelle":"Test"}},
        "claw":{"offen_in":1.0,"pin_in":0.2,"max_in_s":4.0}}},
      "b":{"mechanismen":{
        "lift":{"min_deg":0,"max_deg":100,"max_deg_s":90,"max_deg_s2":500},
        "claw":{"offen_in":1.0,"pin_in":0.2,"max_in_s":4.0}}}}})";
    const Json j = parse_json(text);
    const MechanismParams a = load_mechanism_params(j, "a");
    const MechanismParams b = load_mechanism_params(j, "b");
    REQUIRE(a.height.valid());
    CHECK(a.height.at(100.0) == doctest::Approx(49.0));
    CHECK_FALSE(b.height.valid());
}

TEST_CASE("Mechanismen laden: unplausible hoehe (fallend) wirft") {
    const char* text = R"({"roboter":{"a":{"mechanismen":{
        "lift":{"min_deg":0,"max_deg":100,"max_deg_s":90,"max_deg_s2":500,
                "hoehe":{"winkel_deg":[0,25,50,75,100],"hoehe_in":[9,19,29,19,49]}},
        "claw":{"offen_in":1.0,"pin_in":0.2,"max_in_s":4.0}}}}})";
    CHECK_THROWS(load_mechanism_params(parse_json(text), "a"));
}

TEST_CASE("Game: lift_height_in folgt dem Lift-Winkel, ohne Tabelle leer") {
    SimConfig cfg = match_cfg(40.0);
    Game game(cfg.game, 1, 2, 60000);
    REQUIRE(game.lift_height_in(0).has_value());
    CHECK(*game.lift_height_in(0) == doctest::Approx(10.0));  // untere Endlage
    CHECK_FALSE(game.lift_height_in(1).has_value());
}

TEST_CASE("Match: Lift über 50 Zoll meldet RULE:SG3 genau einmal je Überschreitung") {
    SimConfig cfg = match_cfg(60.0);
    std::ostringstream s, r;
    const RunResult res = run_match(cfg, 1, s, r);
    CHECK(count(s.str(), "RULE:SG3") == 1);
    CHECK(count(r.str(), "RULE:SG3") == 0);  // Runner ohne Höhentabelle: nicht modelliert, nichts erfunden
    CHECK(res.rule_violations == 1);
}

TEST_CASE("Match: Lift unter 50 Zoll bleibt ohne Regelmeldung") {
    SimConfig cfg = match_cfg(40.0);
    std::ostringstream s, r;
    const RunResult res = run_match(cfg, 1, s, r);
    CHECK(count(s.str(), "RULE:") == 0);
    CHECK(res.rule_violations == 0);
}

TEST_CASE("Match: genau 50 Zoll ist erlaubt (SG3: höchstens 50)") {
    SimConfig cfg = match_cfg(50.0);
    std::ostringstream s, r;
    run_match(cfg, 1, s, r);
    CHECK(count(s.str(), "RULE:SG3") == 0);
}
