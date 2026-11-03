#pragma once

#include <string>
#include <vector>

#include "util/rule_guard.hpp"

// Regelprüfer der Simulation (SIM-05): prüft die Spielregeln aus CLAUDE.md gegen den Zustand eines Roboters.
// SG2 Footprint höchstens 24" x 24", SG3 Höhe höchstens 50", SG6 Besitz höchstens 1 Pin + 1 Cup; bei Matchende
// (0:00) stehen alle Motoren. Die Punktewertung (RSC3) ist nicht Teil dieser Datei.
namespace frostbyte::sim {

// Grenzen aus lib/util/rule_guard.hpp (eine Quelle, M-07/ADR-027).
constexpr double max_footprint_in = kMaxFootprintIn;  // SG2
constexpr double max_height_in = kMaxHeightIn;        // SG3
constexpr int max_pins = kMaxPins;                    // SG6
constexpr int max_cups = kMaxCups;                    // SG6

struct RobotRuleState {
    double length_in = 0.0;  // Footprint (ausgefahren, größte Ausdehnung)
    double width_in = 0.0;
    double height_in = 0.0;
    int pins = 0;
    int cups = 0;
};

struct RuleViolation {
    std::string rule;  // "SG2", "SG3", "SG6"
    std::string text;
};

std::vector<RuleViolation> check_rules(const RobotRuleState& s);

// true, wenn alle Motorbefehle (Spannung oder Prozent) null sind. Gilt für Matchende 0:00.
bool motors_stopped(const std::vector<double>& commands);

}  // namespace frostbyte::sim
