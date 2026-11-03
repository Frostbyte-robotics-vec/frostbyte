#include <sstream>
#include <string>
#include <vector>

#include "doctest/doctest.h"
#include "sim/game_state.hpp"
#include "sim/json.hpp"
#include "sim/world.hpp"

using namespace frostbyte::sim;

// Erwartungswerte sind Handrechnungen aus docs/rules_scoring.md (Nummern in den Testnamen). Eigene Goals, Posen und
// Midfield-Lage im Test: nichts hängt an Platzhaltern aus field.json oder sim_params.json.
namespace {

const std::string fixtures = FB_TEST_FIXTURES;
using C = PinColor;

GameSetup two_goals() {
    GameSetup g;
    g.goals = {{"RED_A", "RED_1"}, {"BLUE_A", "BLUE_1"}, {"MID", "MIDFIELD"}};
    return g;
}

GameEvent pick_pin(int t_ms, C lower, C upper, int robot = 0) {
    GameEvent e;
    e.t_ms = t_ms;
    e.robot = robot;
    e.kind = GameEventKind::pick_pin;
    e.lower = lower;
    e.upper = upper;
    return e;
}
GameEvent pick_cup(int t_ms, bool opaque_lower, int robot = 0) {
    GameEvent e;
    e.t_ms = t_ms;
    e.robot = robot;
    e.kind = GameEventKind::pick_cup;
    e.opaque_lower = opaque_lower;
    return e;
}
GameEvent place(int t_ms, GameEventKind k, const std::string& goal, int robot = 0) {
    GameEvent e;
    e.t_ms = t_ms;
    e.robot = robot;
    e.kind = k;
    e.target = goal;
    return e;
}
GameEvent toggle(int t_ms, const std::string& quadrant, ToggleColor c) {
    GameEvent e;
    e.t_ms = t_ms;
    e.kind = GameEventKind::set_toggle;
    e.target = quadrant;
    e.toggle = c;
    return e;
}

std::vector<std::string> split_csv(const std::string& s) {
    std::vector<std::string> out(1);
    for (char c : s) {
        if (c == ',')
            out.emplace_back();
        else
            out.back() += c;
    }
    return out;
}

// Spalte `name` der Zeile mit t_s == t aus einer Roboter-CSV (Log v1); "?" wenn es die Zeile nicht gibt.
std::string cell(const std::string& csv, const std::string& t, const std::string& name) {
    std::istringstream in(csv);
    std::string line;
    std::vector<std::string> cols;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        if (cols.empty()) {
            cols = split_csv(line);
            continue;
        }
        const auto f = split_csv(line);
        if (f[0] == t)
            for (size_t i = 0; i < cols.size(); ++i)
                if (cols[i] == name)
                    return f[i];
    }
    return "?";
}

SimConfig scenario(const GameSetup& game) {
    SimConfig cfg = load_config(fixtures + "/sim_params_min.json", fixtures + "/field_min.json",
                                fixtures + "/empty.route", fixtures + "/empty.route", "none");
    cfg.game = game;
    return cfg;
}

}  // namespace

TEST_CASE("Spielzustand 2: Pin im roten Goal, gelbe Hälfte zählt erst mit rotem Toggle") {
    GameState g(two_goals());
    CHECK(g.apply(pick_pin(0, C::red, C::yellow)).empty());
    CHECK(g.apply(place(0, GameEventKind::place_pin, "RED_A")).empty());
    CHECK(g.score(0) == 0);
    g.apply(toggle(0, "RED_1", ToggleColor::blue));
    CHECK(g.score(0) == 0);
    g.apply(toggle(0, "RED_1", ToggleColor::red));
    CHECK(g.score(0) == 10);
}

TEST_CASE("Spielzustand 3: Stapel Pin, Cup, Pin im blauen Goal (Cup: untere Hälfte durchsichtig)") {
    GameState g(two_goals());
    g.apply(toggle(0, "BLUE_1", ToggleColor::blue));
    g.apply(pick_pin(0, C::red, C::blue));
    g.apply(place(0, GameEventKind::place_pin, "BLUE_A"));
    g.apply(pick_cup(0, false));
    g.apply(place(0, GameEventKind::place_cup, "BLUE_A"));
    g.apply(pick_pin(0, C::yellow, C::red));
    g.apply(place(0, GameEventKind::place_pin, "BLUE_A"));
    CHECK(g.score(0) == 5);
}

TEST_CASE("Spielzustand 4: Midfield-Goal, zwei Roboter im Midfield gibt 31") {
    GameState g(two_goals());
    g.apply(pick_pin(0, C::yellow, C::red));
    g.apply(place(0, GameEventKind::place_pin, "MID"));
    g.apply(pick_cup(0, true));
    g.apply(place(0, GameEventKind::place_cup, "MID"));
    g.apply(pick_pin(0, C::blue, C::yellow));
    g.apply(place(0, GameEventKind::place_pin, "MID"));
    CHECK(g.score(0) == 5);  // ohne Roboter im Midfield ist gelb nicht Owned: nur die blaue Hälfte
    CHECK(g.score(2) == 31);
}

TEST_CASE("Spielzustand: SG6 - ein zweiter Pin wird nicht aufgenommen und meldet RULE:SG6") {
    GameState g(two_goals());
    CHECK(g.apply(pick_pin(0, C::red, C::red)).empty());
    CHECK(g.apply(pick_cup(0, false)).empty());
    const auto v = g.apply(pick_pin(0, C::blue, C::blue));
    REQUIRE(v.size() == 1);
    CHECK(v[0] == "RULE:SG6");
    CHECK(g.pins(0) == 1);
    CHECK(g.cups(0) == 1);
    CHECK(g.pins(1) == 0);  // der andere Roboter hat eigenen Besitz
    CHECK(g.apply(pick_pin(0, C::blue, C::blue, 1)).empty());
}

TEST_CASE("Spielzustand: Szenariofehler werfen") {
    GameState g(two_goals());
    CHECK_THROWS_AS(g.apply(place(0, GameEventKind::place_pin, "RED_A")), std::runtime_error);  // nichts in der Hand
    g.apply(pick_pin(0, C::red, C::red));
    CHECK_THROWS_AS(g.apply(place(0, GameEventKind::place_pin, "NIRGENDWO")), std::runtime_error);
    CHECK_THROWS_AS(g.apply(toggle(0, "RED_9", ToggleColor::red)), std::runtime_error);
    GameSetup s = two_goals();
    s.events = {toggle(500, "RED_1", ToggleColor::red), toggle(100, "RED_1", ToggleColor::blue)};
    CHECK_THROWS_AS(GameState{s}, std::runtime_error);  // Zeiten nicht aufsteigend
    GameSetup bad;
    bad.goals = {{"X", "GREEN_1"}};
    CHECK_THROWS_AS(GameState{bad}, std::runtime_error);
}

TEST_CASE("Midfield: Quadrat ist um 45 Grad gedreht, Berühren eines Teils genügt") {
    MidfieldSpec m;
    m.present = true;
    m.side_in = 48.0;  // Halbdiagonale 33.94
    const auto box = [](double x, double y) { return Box{x, y, 24.0, 24.0, 0.0}; };
    CHECK(robots_in_midfield(m, {box(0, 0)}) == 1);
    CHECK(robots_in_midfield(m, {box(40, 0)}) == 1);  // Spitze des Rhombus ragt unter die Kante
    CHECK(robots_in_midfield(m, {box(50, 0)}) == 0);
    CHECK(robots_in_midfield(m, {box(30, 30)}) == 0);  // läge im ungedrehten Quadrat (Ecke 18/18), nicht im gedrehten
    CHECK(robots_in_midfield(m, {box(0, 0), box(0, 10), box(5, 5)}) == 2);  // höchstens zwei zählen
    m.present = false;
    CHECK(robots_in_midfield(m, {box(0, 0)}) == 0);
}

TEST_CASE("field.json lesen: Goals, Quadranten, Midfield und gelbe Startpins auf Goals") {
    const Json f = parse_json(R"({
      "goals": [
        {"id": "G1", "quadrant": "RED_1"},
        {"id": "MIDFIELD", "quadrant": "MIDFIELD"}],
      "midfield": {"seite_in": 48, "mitte_x_in": 1, "mitte_y_in": 2},
      "objekte_start": {"pins": [
        {"id": "P0", "variante": "yellow", "x_in": 0, "y_in": 0},
        {"id": "P1", "variante": "yellow", "auf_goal": "G1"}]}
    })");
    GameSetup g = load_game_setup(f);
    REQUIRE(g.goals.size() == 2);
    CHECK(g.goals[0].id == "G1");
    CHECK(g.goals[0].quadrant == "RED_1");
    CHECK(g.midfield.present);
    CHECK(g.midfield.side_in == 48.0);
    CHECK(g.midfield.x_in == 1.0);
    REQUIRE(g.start.size() == 1);  // nur der Pin mit auf_goal liegt schon im Stapel
    CHECK(g.start[0].goal == "G1");
    GameState s(g);
    CHECK(s.score(0) == 0);
    s.apply(toggle(0, "RED_1", ToggleColor::red));
    CHECK(s.score(0) == 10);  // Pin(gelb, gelb): obere Hälfte sichtbar und Owned
    CHECK(load_game_setup(parse_json(R"({"feld_in": 144})")).empty());
}

TEST_CASE("Welt: score-Spalte folgt den Ereignissen, in beiden Logs gleich; byte-gleich bei gleichem Seed") {
    GameSetup g = two_goals();
    g.events = {pick_pin(1000, C::red, C::yellow), place(2000, GameEventKind::place_pin, "RED_A"),
                toggle(3000, "RED_1", ToggleColor::red)};
    const SimConfig cfg = scenario(g);
    std::ostringstream s1, r1, s2, r2;
    const RunResult res = run_match(cfg, 7, s1, r1);
    run_match(cfg, 7, s2, r2);
    CHECK(s1.str() == s2.str());
    CHECK(r1.str() == r2.str());
    CHECK(res.rule_violations == 0);
    CHECK(cell(s1.str(), "0.500", "score") == "0");
    CHECK(cell(s1.str(), "2.500", "score") == "0");
    CHECK(cell(s1.str(), "3.000", "score") == "10");
    CHECK(cell(s1.str(), "60.000", "score") == "10");
    CHECK(cell(r1.str(), "60.000", "score") == "10");
    // Besitz im Log: Pin in der Hand zwischen 1 s und 2 s
    CHECK(cell(s1.str(), "1.500", "has_pin") == "1");
    CHECK(cell(s1.str(), "2.500", "has_pin") == "0");
    CHECK(cell(s1.str(), "1.500", "has_cup") == "0");
}

TEST_CASE("Welt: RULE:SG6 aus dem Spielzustand zählt als Regelverstoß und steht im Log") {
    GameSetup g = two_goals();
    g.events = {pick_pin(100, C::red, C::red), pick_pin(200, C::blue, C::blue)};
    std::ostringstream s, r;
    const RunResult res = run_match(scenario(g), 1, s, r);
    CHECK(res.rule_violations == 1);
    CHECK(s.str().find("RULE:SG6") != std::string::npos);
}

TEST_CASE("Welt: Roboter im Midfield bringt 8, auch ohne Ereignisse") {
    GameSetup g = two_goals();
    g.midfield.present = true;
    g.midfield.x_in = -60.0;  // Stacker startet in field_min.json bei (-60, 12.5)
    g.midfield.y_in = 12.5;
    g.midfield.side_in = 48.0;
    std::ostringstream s, r;
    run_match(scenario(g), 1, s, r);
    CHECK(cell(s.str(), "0.000", "score") == "8");
    CHECK(cell(r.str(), "0.000", "score") == "8");
}

TEST_CASE("Welt: ohne Spielzustand bleibt die score-Spalte leer") {
    std::ostringstream s, r;
    run_match(scenario(GameSetup{}), 1, s, r);
    CHECK(cell(s.str(), "1.000", "score").empty());
    CHECK(cell(s.str(), "1.000", "has_pin").empty());
}
