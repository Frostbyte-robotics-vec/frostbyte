#pragma once

#include <cstdint>
#include <functional>
#include <iosfwd>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "sim/collision.hpp"
#include "sim/drive.hpp"
#include "sim/rng.hpp"
#include "sim/sensor_models.hpp"

// Welt der Simulation: fester 1-ms-Schritt, Steuerung und Log auf ganzzahligen ms-Rastern aus sim_params.json.
// Gleicher Seed = byte-gleiche CSV (keine std::*_distribution, kein Aufaddieren von Gleitkommazeit).
namespace frostbyte::sim {

// Skills-Match: 60 s (Spielregel, kein Modellwert).
constexpr int match_ms = 60000;

struct RobotSetup {
    std::string name;                // "stacker" oder "runner"
    std::optional<Pose> start;       // leer, solange field.json null-Koordinaten hat (A-01)
    std::vector<std::string> route;  // Routenzeilen ohne Kommentare; interpretiert erst ab dem Routen-Ticket
    DriveParams drive;               // roboter.<name> aus sim_params.json (SIM-02)
    SensorParams sensors;            // roboter.<name>.sensoren, skaliert mit dem Rauschprofil (SIM-03)
    double length_in = 0.0;          // roboter.<name>.umriss_in (Kollision, SG2)
    double width_in = 0.0;
    // Handsteuerung bis zum Routen-Ticket: Spannung links/rechts zur Zeit t_ms. Leer = 0 V.
    std::function<std::pair<double, double>(int t_ms)> open_loop;
    // Regelung (D-05, z. B. sim/motion_adapter.hpp): je Steuertakt mit der geschätzten Pose aufgerufen, die Spannung
    // gilt bis zum nächsten Takt. Gesetzt hat Vorrang vor open_loop. Matchende 0:00 wie bei open_loop.
    std::function<std::pair<double, double>(int t_ms, const Pose& est)> closed_loop;
};

struct SimConfig {
    int schritt_ms = 0;
    int steuer_ms = 0;
    int log_ms = 0;
    std::string build = "-";
    std::string noise_profile = "realistic";  // none | realistic | harsh (rauschen_profile in sim_params.json)
    double field_in = 144.0;                  // Kantenlänge des Feldquadrats; load_config nimmt feld_in aus field.json
    RobotSetup stacker;
    RobotSetup runner;
};

struct RunResult {
    int log_rows = 0;        // je Roboter
    int control_ticks = 0;   // je Roboter
    int wall_contacts = 0;   // Kontakte mit dem Feldrand (je Beginn), beide Roboter zusammen
    int robot_contacts = 0;  // Kontakte der Roboter untereinander (je Beginn)
    int rule_violations = 0;
};

// Liest welt und roboter.<name> (Fahrmodell) aus sim_params.json, Startposen aus field.json (posen START_STACKER/START_RUNNER) und die Routen.
SimConfig load_config(const std::string& params_path, const std::string& field_path, const std::string& stacker_route,
                      const std::string& runner_route, const std::string& noise_profile = "realistic");

std::vector<std::string> load_route(const std::string& path);

// Ein Match. Schreibt je Roboter eine CSV nach docs/log_format.md (Zeilenende '\n').
RunResult run_match(const SimConfig& cfg, std::uint64_t seed, std::ostream& stacker_csv, std::ostream& runner_csv);

}  // namespace frostbyte::sim
