#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "sim/collision.hpp"
#include "sim/json.hpp"
#include "sim/rules.hpp"
#include "sim/scorer.hpp"

// Spielzustand der Simulation (SIM-20, ADR-026): Stapel je Goal, Toggle je Quadrant, Besitz je Roboter.
// Änderungen kommen als Ereignisse mit Zeit (Szenario, später der Routenspieler A-03); der Scorer (SIM-10) rechnet daraus
// den Punktestand. Szenariofehler (unbekanntes Goal, Setzen ohne Besitz, unsortierte Zeiten) sind std::runtime_error.
namespace frostbyte::sim {

enum class GameEventKind { pick_pin, pick_cup, place_pin, place_cup, set_toggle };

struct GameEvent {
    int t_ms = 0;
    int robot = 0;  // 0 = Stacker, 1 = Runner
    GameEventKind kind = GameEventKind::pick_pin;
    PinColor lower = PinColor::red;  // pick_pin: Farbe der unteren/oberen Hälfte
    PinColor upper = PinColor::red;
    bool opaque_lower = false;  // pick_cup: welche Cup-Hälfte undurchsichtig ist (ADR-024)
    std::string target;         // place_*: Goal-ID; set_toggle: Quadrant (z. B. RED_1)
    ToggleColor toggle = ToggleColor::neutral;
};

struct GoalSpec {
    std::string id;
    std::string quadrant;  // RED_1, BLUE_2 usw. oder MIDFIELD
};

// Midfield: Quadrat mit Kantenlänge side_in um (x, y), um 45° gedreht. present = false: nicht vorhanden (zählt 0 Roboter).
struct MidfieldSpec {
    bool present = false;
    double x_in = 0.0;
    double y_in = 0.0;
    double side_in = 0.0;
};

struct StartItem {
    std::string goal;
    StackItem item;
};

struct GameSetup {
    std::vector<GoalSpec> goals;
    MidfieldSpec midfield;
    std::vector<StartItem> start;
    std::vector<GameEvent> events;  // aufsteigend nach t_ms

    // Ohne Goals gibt es keinen Spielzustand: die Simulation lässt score, has_cup und has_pin leer.
    bool empty() const { return goals.empty(); }
};

class GameState {
public:
    explicit GameState(const GameSetup& setup);

    // Wendet ein Ereignis an. Rückgabe: Regelverstöße als Log-Ereignisse (RULE:SG6); dann wurde nichts aufgenommen.
    std::vector<std::string> apply(const GameEvent& e);

    int pins(int robot) const { return hand_[robot].pins; }
    int cups(int robot) const { return hand_[robot].cups; }

    ScoreInput input(int robots_in_midfield) const;
    int score(int robots_in_midfield) const { return score_skills(input(robots_in_midfield)).total; }

private:
    struct Hand {
        int pins = 0;
        int cups = 0;
        StackItem pin;  // Besitz ist höchstens 1 Pin + 1 Cup (SG6)
        StackItem cup;
    };
    std::vector<GoalSpec> specs_;
    std::vector<GoalState> goals_;
    std::map<std::string, ToggleColor> toggles_;
    Hand hand_[2];
    std::size_t goal_index(const std::string& id) const;
};

// Zahl der Roboter (höchstens max_scored_robots), deren Umriss das Midfield berührt (RSC3b: zumindest teilweise).
int robots_in_midfield(const MidfieldSpec& m, const std::vector<Box>& robots);

// Goals, Quadranten, Midfield und Startpins mit auf_goal aus field.json. Fehlen die Goals: leeres Spiel.
GameSetup load_game_setup(const Json& field);

}  // namespace frostbyte::sim
