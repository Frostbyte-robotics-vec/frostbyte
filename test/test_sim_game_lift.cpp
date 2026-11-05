#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

#include "doctest/doctest.h"
#include "sim/game.hpp"
#include "sim/world.hpp"

// Lift im Match (SIM-29, ADR-009 Nachtrag): Game fährt den Lift je Zyklus hoch und danach zurück; run_match schreibt
// lift_deg und lift_set_deg ins Log. Alle Zahlen gehören dem Test (eigene Gelenkgrenzen, Fixtures game_*.json).
namespace {

using namespace frostbyte::sim;

constexpr int end_ms = 60000;
constexpr double lift_min = 0.0;
constexpr double lift_max = 90.0;
constexpr double lift_vel = 90.0;
constexpr double lift_acc = 1000.0;
constexpr double dt_s = 0.001;

GameSetup one_goal() {
    GameSetup g;
    g.goals.push_back(GoalState{Zone::red_quadrant, ToggleColor::red, {}});
    g.goal_names = {"GOAL_RED"};
    EventParams e;
    e.loader = {1.0, 0.0, 0.0};
    e.floor = {2.0, 0.0, 0.0};
    e.yellow_up_loader = 1.0;
    e.yellow_up_floor = 1.0;
    e.grasp_success = 1.0;
    e.place_success_level0 = 1.0;
    e.place_penalty_per_level = 0.0;
    g.events = e;
    MechanismParams m;
    m.lift = JointParams{lift_min, lift_max, lift_vel, lift_acc};
    m.claw = ClawParams{1.0, 0.2, 4.0};
    g.mech[0] = g.mech[1] = m;
    return g;
}

struct Trace {
    std::vector<double> pos;
    std::vector<double> set;
    int next_ms = 0;
    void run(Game& game, int to_ms) {
        for (; next_ms <= to_ms; ++next_ms) {
            game.step(next_ms);
            pos.push_back(game.lift_deg(0));
            set.push_back(game.lift_set_deg(0));
        }
    }
};

// Drei Zyklen nacheinander (Preload, dann Loader, Loader): Ebenen 0, 1, 2.
int build_three(Game& game, Trace& tr) {
    CycleTicket t = game.start_cycle(500, 0, 0, game_level_auto, true);
    REQUIRE(t.accepted);
    for (int i = 0; i < 2; ++i) {
        tr.run(game, t.done_ms);
        const CycleTicket in = game.start_intake(t.done_ms, 0, Source::loader);
        REQUIRE(in.accepted);
        tr.run(game, in.done_ms);
        t = game.start_cycle(in.done_ms, 0, 0, game_level_auto, false);
        REQUIRE(t.accepted);
    }
    tr.run(game, t.done_ms + 3000);
    return t.done_ms;
}

struct Rows {
    std::vector<std::string> head;
    std::vector<std::vector<std::string>> r;
    int col(const std::string& name) const {
        for (std::size_t i = 0; i < head.size(); ++i)
            if (head[i] == name)
                return static_cast<int>(i);
        return -1;
    }
    std::vector<std::string> column(const std::string& name) const {
        std::vector<std::string> out;
        const int c = col(name);
        REQUIRE(c >= 0);
        for (const auto& row : r)
            out.push_back(row[static_cast<std::size_t>(c)]);
        return out;
    }
};

Rows parse(const std::string& csv) {
    Rows out;
    std::stringstream in(csv);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::vector<std::string> cells;
        std::string cell;
        std::stringstream ls(line);
        while (std::getline(ls, cell, ','))
            cells.push_back(cell);
        if (!line.empty() && line.back() == ',')
            cells.push_back("");
        if (out.head.empty())
            out.head = cells;
        else
            out.r.push_back(cells);
    }
    return out;
}

SimConfig load(const std::string& stacker, const std::string& runner) {
    const std::string fx = FB_TEST_FIXTURES;
    SimConfig cfg = load_config(fx + "/sim_params_game.json", fx + "/field_game.json", fx + "/" + stacker,
                                fx + "/" + runner, "none");
    return cfg;
}

}  // namespace

TEST_CASE("Lift: steht vor dem ersten Zyklus auf der unteren Endlage, Sollwert gleich") {
    Game game(one_goal(), 1, 2, end_ms);
    Trace tr;
    tr.run(game, 100);
    for (std::size_t i = 0; i < tr.pos.size(); ++i) {
        CHECK(tr.pos[i] == lift_min);
        CHECK(tr.set[i] == lift_min);
    }
}

TEST_CASE("Lift: fährt Ebene für Ebene höher, bleibt in den Endlagen und kehrt nach dem Zyklus zurück") {
    Game game(one_goal(), 1, 2, end_ms);
    Trace tr;
    build_three(game, tr);
    const double top = *std::max_element(tr.pos.begin(), tr.pos.end());
    CHECK(top == doctest::Approx(lift_min + (lift_max - lift_min) * 2 / game_max_level));  // Ebene 2
    CHECK(*std::max_element(tr.set.begin(), tr.set.end()) == doctest::Approx(top));
    for (double p : tr.pos) {
        CHECK(p >= lift_min);
        CHECK(p <= lift_max);
    }
    CHECK(tr.pos.back() == lift_min);  // wieder unten
    CHECK(tr.set.back() == lift_min);
}

TEST_CASE("Lift: Geschwindigkeits- und Beschleunigungsgrenze gelten Schritt für Schritt") {
    Game game(one_goal(), 1, 2, end_ms);
    Trace tr;
    build_three(game, tr);
    double v_prev = 0.0;
    for (std::size_t i = 1; i < tr.pos.size(); ++i) {
        const double v = (tr.pos[i] - tr.pos[i - 1]) / dt_s;
        CHECK(std::abs(v) <= lift_vel + 1e-6);
        CHECK(std::abs(v - v_prev) <= 2.0 * lift_acc * dt_s + 1e-6);  // Einrasten: höchstens 2*a*dt (ADR-009)
        v_prev = v;
    }
}

TEST_CASE("Lift: Abbruch (Timeout) fährt den Lift zurück, der Sollwert fällt sofort auf unten") {
    Game game(one_goal(), 1, 2, end_ms);
    Trace tr;
    int t = 500;
    CycleTicket c = game.start_cycle(t, 0, 0, game_level_auto, true);
    for (int i = 0; i < 2; ++i) {
        tr.run(game, c.done_ms);
        const CycleTicket in = game.start_intake(c.done_ms, 0, Source::loader);
        tr.run(game, in.done_ms);
        c = game.start_cycle(in.done_ms, 0, 0, game_level_auto, false);
    }
    // Mitten im Hub des dritten Zyklus abbrechen.
    int cancel_ms = -1;
    for (int ms = c.done_ms - 1000; ms < c.done_ms; ++ms) {
        tr.run(game, ms);
        if (game.lift_set_deg(0) > lift_min) {
            cancel_ms = ms;
            break;
        }
    }
    REQUIRE(cancel_ms >= 0);
    game.cancel(cancel_ms, 0);
    CHECK(game.lift_set_deg(0) == lift_min);
    tr.run(game, cancel_ms + 2000);
    CHECK(tr.pos.back() == lift_min);
}

TEST_CASE("Lift im Match: Spalten lift_deg und lift_set_deg gefüllt, in den Endlagen, Roboter ohne Route steht") {
    SimConfig cfg = load("game_three_cycles.route", "empty.route");
    std::ostringstream s, r;
    run_match(cfg, 1, s, r);
    const Rows st = parse(s.str());
    const Rows ru = parse(r.str());
    double top = lift_min;
    for (const std::string& v : st.column("lift_deg")) {
        REQUIRE_FALSE(v.empty());
        const double d = std::stod(v);
        CHECK(d >= lift_min);
        CHECK(d <= lift_max);
        top = std::max(top, d);
    }
    for (const std::string& v : st.column("lift_set_deg"))
        REQUIRE_FALSE(v.empty());
    CHECK(top == doctest::Approx(lift_min + (lift_max - lift_min) * 2 / game_max_level).epsilon(0.01));
    for (const std::string& v : ru.column("lift_deg"))
        CHECK(std::stod(v) == lift_min);
}

TEST_CASE("Lift im Match: gleicher Seed = byte-gleiche Logs mit Liftverlauf") {
    const auto run = [](std::uint64_t seed) {
        SimConfig cfg = load("game_three_cycles.route", "empty.route");
        std::ostringstream s, r;
        run_match(cfg, seed, s, r);
        return s.str() + r.str();
    };
    CHECK(run(7) == run(7));
}

TEST_CASE("Lift im Match: ohne Spielzustand bleiben lift_deg und lift_set_deg leer (nichts erfunden)") {
    const std::string fx = FB_TEST_FIXTURES;
    SimConfig cfg = load_config(fx + "/sim_params_min.json", fx + "/field_min.json", "", "");
    std::ostringstream s, r;
    run_match(cfg, 1, s, r);
    const Rows st = parse(s.str());
    for (const std::string& v : st.column("lift_deg"))
        CHECK(v.empty());
    for (const std::string& v : st.column("lift_set_deg"))
        CHECK(v.empty());
}
