#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "sim/collision.hpp"
#include "sim/json.hpp"
#include "sim/mechanism.hpp"
#include "sim/objects.hpp"
#include "sim/scorer.hpp"

// Spielzustand der Simulation (SIM-20, ADR-026): Goals mit Stapel, dazu Zyklen, die ein Szenario (Demo, später der
// Routenspieler) zu festen Zeiten startet. Ein Zyklus läuft über ObjectModel::run_cycle (Dauer und Zufall), das Ergebnis
// landet nach seiner Dauer im Stapel, und score_skills() (SIM-10) rechnet den Punktestand neu. Die Roboter fahren dafür
// nirgends hin: Ort und Zeit der Zyklen sind Vorgaben des Szenarios, nicht aus der Physik abgeleitet.
namespace frostbyte::sim {

constexpr int game_max_level = 5;    // Ebenen 0..5 (ADR-022)
constexpr int game_level_auto = -1;  // Ebene = Zahl der Teile, die schon auf dem Goal liegen (field.json "ebenen")

struct GameCycle {
    int t_ms = 0;   // Start, Matchzeit
    int robot = 0;  // 0 = Stacker, 1 = Runner
    int goal = 0;   // Index in GameSetup::goals
    int level = 0;  // 0..game_max_level
    Source source = Source::loader;
};

// Midfield: Quadrat mit Kantenlänge side_in um (x, y), um 45° gedreht (field.json midfield).
struct MidfieldSpec {
    bool present = false;
    double x_in = 0.0;
    double y_in = 0.0;
    double side_in = 0.0;
};

// Ein Toggle des Spielfelds: der Befehl toggle setzt ihn auf color (Zielfarbe des Quadranten); goals sind die Goals seines
// Quadranten (Indizes in GameSetup::goals).
struct ToggleSpec {
    std::string name;
    ToggleColor color = ToggleColor::neutral;
    std::vector<int> goals;
    double x_in = 0.0;  // Lage aus field.json (SIM-25): Reichweitenprüfung von start_toggle
    double y_in = 0.0;
};

struct GameSetup {
    std::vector<GoalState> goals;         // Anfangszustand (leere Stapel); leer = kein Spielzustand
    std::vector<std::string> goal_names;  // je Goal der Name aus field.json (für Routen, SIM-22); sonst leer
    std::vector<ToggleSpec> toggles;      // je Toggle aus field.json (für Routen, SIM-23); sonst leer
    int toggle_ms = 0;                    // Dauer des Befehls toggle (sim_params spiel.toggle_dauer_s)
    double toggle_reach_in = 0.0;  // größter Abstand Roboter-Toggle für toggle (spiel.toggle_reichweite_in, SIM-25)
    std::vector<GameCycle> cycles;
    EventParams events;
    MechanismParams mech[2];
    // Annahme ohne Regeltext (ADR-024): welche Cup-Hälfte undurchsichtig ist.
    bool cup_opaque_lower = false;
    // Midfield aus field.json (Quadrat, um 45° gedreht); present = false: kein Roboter zählt dort.
    MidfieldSpec midfield;

    bool enabled() const { return !goals.empty(); }
};

// Liest ereignisse, roboter.<name>.mechanismen und spiel aus sim_params.json (Fehler: std::runtime_error).
void load_game_params(const Json& params, GameSetup& g);

// Liest den Block midfield aus field.json (seite_in, mitte_x_in, mitte_y_in); fehlt er, bleibt present = false.
MidfieldSpec load_midfield(const Json& field);

// Zahl der Roboter (höchstens max_scored_robots), deren Umriss das Midfield berührt (RSC3b: zumindest teilweise).
int robots_in_midfield(const MidfieldSpec& m, const std::vector<Box>& robots);

// Antwort auf einen Routenbefehl (SIM-22): accepted = false: der Befehl ist nicht möglich (Roboter beschäftigt, nichts in
// der Hand, Ebene passt nicht), es läuft nichts. Sonst endet er bei done_ms (Matchzeit); ok = false: er scheitert dann.
struct CycleTicket {
    bool accepted = false;
    bool ok = false;
    int done_ms = 0;
};

class Game {
public:
    // seed_stacker/seed_runner: eigene Zufallsströme der beiden ObjectModel (nicht die der Sensoren). end_ms: Matchende;
    // was erst bei oder nach end_ms fertig wird, zählt nicht.
    Game(const GameSetup& setup, std::uint64_t seed_stacker, std::uint64_t seed_runner, int end_ms);

    // Zeit t_ms (aufsteigend): schließt fällige Zyklen ab, startet fällige Zyklen. Pro ms höchstens einmal aufrufen.
    void step(int t_ms);

    // Routenbefehle (SIM-22, ADR-032); t_ms ist die Matchzeit des Aufrufs.
    // Aufnahme einer Einheit vom Loader oder Boden (ObjectModel::pick). Der Roboter hält höchstens eine Einheit.
    CycleTicket start_intake(int t_ms, int robot, Source src);
    // Rest des Zyklus auf Goal goal: Greifer, Lift, Setzen. Die Einheit kommt von der Aufnahme davor, sonst vom Preload
    // (preload = true, nur einmal je Roboter). level: game_level_auto oder genau die Zahl der Teile auf dem Goal.
    CycleTicket start_cycle(int t_ms, int robot, int goal, int level, bool preload);
    // Bricht laufende Aufnahme und laufenden Zyklus des Roboters ab (Timeout): nichts landet im Stapel.
    void cancel(int t_ms, int robot);
    // Index des Goals mit diesem Namen (GameSetup::goal_names), -1 wenn unbekannt.
    int goal_index(const std::string& name) const;
    // Toggle setzen (SIM-23): der Roboter ist toggle_ms beschäftigt, danach steht der Toggle auf seiner Farbe. Gleiche
    // Antwort wie bei Zyklen; ein unbekannter Toggle oder ein beschäftigter Roboter wird abgelehnt.
    CycleTicket start_toggle(int t_ms, int robot, int toggle, double robot_x_in, double robot_y_in);
    // Index des Toggles mit diesem Namen (GameSetup::toggles), -1 wenn unbekannt.
    int toggle_index(const std::string& name) const;
    int rejected_cycles() const { return rejected_; }  // von Routen abgelehnte Befehle

    // Punktestand, wenn das Match jetzt endete, mit so vielen Robotern im Midfield (0..max_scored_robots, sonst begrenzt).
    int score(int robots_in_midfield = 0) const;
    // Besitz je Roboter (SG6: höchstens 1 Pin + 1 Cup): ab Start eines Zyklus bis zu seinem Ende. Ein Roboter fährt nur
    // einen Zyklus zugleich (ein zweiter wird übersprungen), SG6 kann hier nicht überschritten werden.
    int pins(int robot) const { return held_[robot].pins; }
    int cups(int robot) const { return held_[robot].cups; }
    int completed_cycles() const { return completed_; }
    int failed_cycles() const { return failed_; }
    int skipped_cycles() const { return skipped_; }  // gestartet, während der Roboter noch einen Zyklus fuhr

private:
    struct Pending {
        int done_ms;
        int goal;
        bool yellow_up;
        int robot;
    };
    struct PendingToggle {
        int done_ms;
        int toggle;
        int robot;
    };
    struct Held {
        int pins = 0;
        int cups = 0;
    };

    struct Hand {
        bool unit = false;  // eine aufgenommene Einheit (ab unit_ready_ms verfügbar)
        int unit_ready_ms = 0;
        bool unit_yellow_up = false;
        bool preload_used = false;
    };

    void rescore();
    int pieces_on(int goal) const;  // Teile auf dem Goal, die unterwegs mitgezählt
    // Zyklus starten (gemeinsam für Szenario und Route); pick_src leer = Einheit ist schon in der Hand.
    CycleTicket launch(int t_ms, int robot, int goal, int level, const std::optional<Source>& pick_src,
                       bool yellow_up_in_hand);

    GameSetup setup_;
    ObjectModel models_[2];
    int end_ms_;
    std::vector<GoalState> goals_;
    std::vector<bool> started_;
    std::vector<Pending> pending_;
    std::vector<PendingToggle> pending_toggles_;
    int busy_until_[2] = {0, 0};
    Held held_[2];
    int score_[max_scored_robots + 1] = {0, 0, 0};  // je Zahl der Roboter im Midfield
    int completed_ = 0;
    int failed_ = 0;
    int skipped_ = 0;
    int rejected_ = 0;
    Hand hand_[2];
};

}  // namespace frostbyte::sim
