#pragma once

#include <vector>

// Scorer der Simulation (SIM-10): Punkte eines Skills-Matches nach RSC3 (Regeln und Handrechnungen: docs/rules_scoring.md).
// Reine Funktion auf einem beschriebenen Spielzustand; keine Abhängigkeit von field.json. Entscheidungen: ADR-024.
namespace frostbyte::sim {

enum class PinColor { red, blue, yellow };
enum class Zone { red_quadrant, blue_quadrant, midfield };
enum class ToggleColor { red, blue, neutral };  // nicht gesetzt zählt als neutral (SC4)

constexpr int red_blue_half_points = 5;   // RSC3
constexpr int yellow_half_points = 10;    // RSC3
constexpr int midfield_robot_points = 8;  // RSC3b
constexpr int max_scored_robots = 2;      // VEX U: zwei Roboter (VURS2)

// Ein Gegenstand im Stapel eines Goals. Pin: Farbe der unteren und oberen Hälfte. Cup: Welche Hälfte undurchsichtig ist
// (steht im Manual nicht lesbar, daher Eingabe): opaque_lower = true heißt, die untere Hälfte ist undurchsichtig.
struct StackItem {
    enum class Kind { pin, cup };
    Kind kind = Kind::pin;
    PinColor lower = PinColor::red;
    PinColor upper = PinColor::red;
    bool opaque_lower = false;

    static StackItem make_pin(PinColor lower, PinColor upper) { return {Kind::pin, lower, upper, false}; }
    static StackItem make_cup(bool opaque_lower) { return {Kind::cup, PinColor::red, PinColor::red, opaque_lower}; }
};

// Ein Goal mit seinem Stapel von unten nach oben. toggle ist der Toggle des Quadranten (bei midfield ohne Bedeutung).
struct GoalState {
    Zone zone = Zone::midfield;
    ToggleColor toggle = ToggleColor::neutral;
    std::vector<StackItem> stack;
};

struct ScoreInput {
    std::vector<GoalState> goals;
    int robots_in_midfield = 0;  // wird auf 0..max_scored_robots begrenzt
};

struct ScoreResult {
    int red_blue_halves = 0;
    int yellow_halves = 0;
    int red_blue_points = 0;
    int yellow_points = 0;
    int midfield_robot_points = 0;
    int total = 0;
};

ScoreResult score_skills(const ScoreInput& in);

}  // namespace frostbyte::sim
