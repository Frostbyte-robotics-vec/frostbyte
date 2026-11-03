#include <sstream>
#include <string>
#include <vector>

#include "doctest/doctest.h"
#include "sim/demo.hpp"
#include "sim/scorer.hpp"
#include "sim/world.hpp"

using namespace frostbyte::sim;

namespace {

const std::string root = FB_ROOT_DIR;
const std::string fixtures = FB_TEST_FIXTURES;

// Letzte Spalte "score" je Zeile; leer = ""
std::vector<std::string> score_column(const std::string& csv) {
    std::vector<std::string> out;
    std::istringstream in(csv);
    std::string line;
    int score_idx = -1;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::vector<std::string> cells;
        std::string cell;
        std::istringstream ls(line);
        while (std::getline(ls, cell, ','))
            cells.push_back(cell);
        if (line.back() == ',')
            cells.push_back("");
        if (score_idx < 0) {
            for (std::size_t i = 0; i < cells.size(); ++i)
                if (cells[i] == "score")
                    score_idx = static_cast<int>(i);
            continue;
        }
        out.push_back(cells[static_cast<std::size_t>(score_idx)]);
    }
    return out;
}

SimConfig demo_with_game() {
    SimConfig cfg = load_config(root + "/sim/sim_params.json", root + "/field/field.json", "", "");
    apply_demo(cfg, root + "/sim/sim_params.json");
    // Annahmen und Platzhalter festnageln: die Tests hängen nicht an `schaetzung`-Werten oder der Demo-Fahrt.
    cfg.game.cup_opaque_lower = false;
    cfg.game.midfield.present = false;
    return cfg;
}

}  // namespace

TEST_CASE("Demo ohne Spielzustand: die Spalte score bleibt leer (kein erfundener Punktestand)") {
    SimConfig cfg = load_config(fixtures + "/sim_params_min.json", fixtures + "/field_min.json", "", "");
    apply_demo(cfg);
    std::ostringstream s, r;
    run_match(cfg, 1, s, r);
    for (const std::string& v : score_column(s.str()))
        CHECK(v.empty());
}

TEST_CASE("Demo mit Spielzustand: score steht in jeder Zeile beider Roboter, und ist gleich") {
    SimConfig cfg = demo_with_game();
    std::ostringstream s, r;
    run_match(cfg, 1, s, r);
    const auto a = score_column(s.str());
    const auto b = score_column(r.str());
    REQUIRE(a.size() == b.size());
    REQUIRE_FALSE(a.empty());
    for (std::size_t i = 0; i < a.size(); ++i) {
        REQUIRE_FALSE(a[i].empty());
        CHECK(a[i] == b[i]);
    }
    CHECK(std::stoi(a.front()) == 0);
}

TEST_CASE("Demo mit Spielzustand: gleicher Seed = byte-gleiche Logs, Spielzufall verschiebt die Posen nicht") {
    const auto run = [](bool with_game, std::uint64_t seed) {
        SimConfig cfg = with_game ? demo_with_game()
                                  : load_config(root + "/sim/sim_params.json", root + "/field/field.json", "", "");
        if (!with_game)
            apply_demo(cfg);
        std::ostringstream s, r;
        run_match(cfg, seed, s, r);
        return s.str() + "|" + r.str();
    };
    CHECK(run(true, 3) == run(true, 3));
    // Ohne die score-Spalte sind die Logs gleich: der Spielzufall zieht aus eigenen Strömen.
    const auto strip = [](const std::string& csv) {
        std::istringstream in(csv);
        std::string line, out;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#' || line.rfind("t_s", 0) == 0) {
                out += line + "\n";
                continue;
            }
            std::vector<std::string> cells;
            std::string cell;
            std::istringstream ls(line);
            while (std::getline(ls, cell, ','))
                cells.push_back(cell);
            if (line.back() == ',')
                cells.push_back("");
            if (cells.size() < 23)
                cells.resize(23);                    // endet die Zeile auf leeren Zellen, fehlen sie hier
            cells[19] = cells[20] = cells[22] = "";  // has_cup, has_pin, score
            for (const std::string& c : cells)
                out += c + ",";
            out += "\n";
        }
        return out;
    };
    CHECK(strip(run(true, 3)) == strip(run(false, 3)));
}

TEST_CASE("Spielzustand: exakter Endstand im Log, gleich der Handrechnung (Pin, Cup, Pin, Cup, Pin je Goal)") {
    // Feste Ereignisparameter (keine Platzhalter aus sim_params.json): nichts scheitert, jeder Pin liegt gelb oben.
    SimConfig cfg = load_config(fixtures + "/sim_params_min.json", fixtures + "/field_min.json", "", "");
    GameSetup& g = cfg.game;
    g.events.loader = {1.0, 0.0, 0.0};
    g.events.floor = {1.0, 0.0, 0.0};
    g.events.yellow_up_loader = 1.0;
    g.events.yellow_up_floor = 1.0;
    g.events.grasp_success = 1.0;
    g.events.place_success_level0 = 1.0;
    g.events.place_penalty_per_level = 0.0;
    for (MechanismParams& m : g.mech) {
        m.lift = {0.0, 100.0, 200.0, 0.0};
        m.claw = {4.0, 2.0, 20.0};
    }
    g.goals.push_back(GoalState{Zone::red_quadrant, ToggleColor::red, {}});
    g.goals.push_back(GoalState{Zone::blue_quadrant, ToggleColor::blue, {}});
    g.cup_opaque_lower = false;
    for (int k = 0; k < 5; ++k) {
        g.cycles.push_back({5000 + k * 10000, 0, 0, 0, Source::loader});
        g.cycles.push_back({7000 + k * 10000, 1, 1, 0, Source::loader});
    }

    // Von Hand: je Goal Pin(eigene Farbe, gelb), Cup, Pin, Cup, Pin. Untere Pinhälften stecken im Goal bzw. in der
    // undurchsichtigen oberen Cup-Hälfte, die oberen gelben Hälften sind sichtbar und Owned: 3 x 10 je Goal.
    const auto stack = [](PinColor own) {
        return std::vector<StackItem>{StackItem::make_pin(own, PinColor::yellow), StackItem::make_cup(false),
                                      StackItem::make_pin(own, PinColor::yellow), StackItem::make_cup(false),
                                      StackItem::make_pin(own, PinColor::yellow)};
    };
    ScoreInput in;
    in.goals.push_back(GoalState{Zone::red_quadrant, ToggleColor::red, stack(PinColor::red)});
    in.goals.push_back(GoalState{Zone::blue_quadrant, ToggleColor::blue, stack(PinColor::blue)});
    const int expected = score_skills(in).total;
    CHECK(expected == 60);

    std::ostringstream s, r;
    run_match(cfg, 1, s, r);
    const auto a = score_column(s.str());
    const auto b = score_column(r.str());
    REQUIRE_FALSE(a.empty());
    CHECK(std::stoi(a.back()) == expected);
    CHECK(std::stoi(b.back()) == expected);
    CHECK(std::stoi(a.front()) == 0);
}

TEST_CASE("Welt: Roboter im Midfield bringt 8 in beiden Logs, Besitz steht in has_cup und has_pin") {
    SimConfig cfg = load_config(fixtures + "/sim_params_min.json", fixtures + "/field_min.json", "", "", "none");
    GameSetup& g = cfg.game;
    g.events.loader = {1.0, 0.0, 0.0};
    g.events.yellow_up_loader = 1.0;
    g.events.grasp_success = 1.0;
    g.events.place_success_level0 = 1.0;
    for (MechanismParams& m : g.mech) {
        m.lift = {0.0, 100.0, 200.0, 0.0};
        m.claw = {4.0, 2.0, 20.0};
    }
    g.goals.push_back(GoalState{Zone::red_quadrant, ToggleColor::red, {}});
    g.cycles.push_back({5000, 0, 0, 0, Source::loader});
    // Der Stacker steht laut field_min.json bei (-60, 12.5) und fährt nicht: das Midfield um diesen Punkt zählt ihn.
    g.midfield = MidfieldSpec{true, -60.0, 12.5, 48.0};
    std::ostringstream s, r;
    run_match(cfg, 1, s, r);
    CHECK(std::stoi(score_column(s.str()).front()) == 8);
    CHECK(std::stoi(score_column(r.str()).front()) == 8);
    CHECK(std::stoi(score_column(s.str()).back()) == 18);  // 8 + Pin(rot, gelb) im roten Goal bei Toggle rot
    // Besitz: Pin in der Hand des Stackers ab 5 s (has_pin = 1), beim Runner nie
    const auto held = [](const std::string& csv, const std::string& col) {
        std::vector<std::string> out;
        std::istringstream in(csv);
        std::string line;
        int idx = -1;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#')
                continue;
            std::vector<std::string> cells(1);
            for (char ch : line) {
                if (ch == ',')
                    cells.emplace_back();
                else
                    cells.back() += ch;
            }
            if (idx < 0) {
                for (std::size_t i = 0; i < cells.size(); ++i)
                    if (cells[i] == col)
                        idx = static_cast<int>(i);
                continue;
            }
            out.push_back(cells[static_cast<std::size_t>(idx)]);
        }
        return out;
    };
    const auto stacker_pin = held(s.str(), "has_pin");
    const auto runner_pin = held(r.str(), "has_pin");
    REQUIRE(stacker_pin.size() == runner_pin.size());
    CHECK(stacker_pin.front() == "0");
    CHECK(stacker_pin[251] == "1");  // t = 5.020 s bei log_ms = 20
    CHECK(stacker_pin.back() == "0");
    for (const std::string& v : runner_pin)
        CHECK(v == "0");
}
