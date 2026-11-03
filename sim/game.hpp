#pragma once

#include <cstdint>
#include <vector>

#include "sim/json.hpp"
#include "sim/mechanism.hpp"
#include "sim/objects.hpp"
#include "sim/scorer.hpp"

// Spielzustand der Simulation (SIM-20, ADR-026): Goals mit Stapel, dazu Zyklen, die ein Szenario (Demo, später der
// Routenspieler) zu festen Zeiten startet. Ein Zyklus läuft über ObjectModel::run_cycle (Dauer und Zufall), das Ergebnis
// landet nach seiner Dauer im Stapel, und score_skills() (SIM-10) rechnet den Punktestand neu. Die Roboter fahren dafür
// nirgends hin: Ort und Zeit der Zyklen sind Vorgaben des Szenarios, nicht aus der Physik abgeleitet.
namespace frostbyte::sim {

constexpr int game_max_level = 5;  // Ebenen 0..5 (ADR-022)

struct GameCycle {
    int t_ms = 0;   // Start, Matchzeit
    int robot = 0;  // 0 = Stacker, 1 = Runner
    int goal = 0;   // Index in GameSetup::goals
    int level = 0;  // 0..game_max_level
    Source source = Source::loader;
};

struct GameSetup {
    std::vector<GoalState> goals;  // Anfangszustand (leere Stapel); leer = kein Spielzustand
    std::vector<GameCycle> cycles;
    EventParams events;
    MechanismParams mech[2];
    // Annahmen ohne Regeltext (ADR-024): welche Cup-Hälfte undurchsichtig ist; Roboter im Midfield bei Matchende.
    bool cup_opaque_lower = false;
    int robots_in_midfield = 0;

    bool enabled() const { return !goals.empty(); }
};

// Liest ereignisse, roboter.<name>.mechanismen und spiel aus sim_params.json (Fehler: std::runtime_error).
void load_game_params(const Json& params, GameSetup& g);

class Game {
public:
    // seed_stacker/seed_runner: eigene Zufallsströme der beiden ObjectModel (nicht die der Sensoren). end_ms: Matchende;
    // was erst bei oder nach end_ms fertig wird, zählt nicht.
    Game(const GameSetup& setup, std::uint64_t seed_stacker, std::uint64_t seed_runner, int end_ms);

    // Zeit t_ms (aufsteigend): schließt fällige Zyklen ab, startet fällige Zyklen. Pro ms höchstens einmal aufrufen.
    void step(int t_ms);

    int score() const { return score_; }
    int completed_cycles() const { return completed_; }
    int failed_cycles() const { return failed_; }
    int skipped_cycles() const { return skipped_; }  // gestartet, während der Roboter noch einen Zyklus fuhr

private:
    struct Pending {
        int done_ms;
        int goal;
        bool yellow_up;
    };

    void rescore();

    GameSetup setup_;
    ObjectModel models_[2];
    int end_ms_;
    std::vector<GoalState> goals_;
    std::vector<bool> started_;
    std::vector<Pending> pending_;
    int busy_until_[2] = {0, 0};
    int score_ = 0;
    int completed_ = 0;
    int failed_ = 0;
    int skipped_ = 0;
};

}  // namespace frostbyte::sim
