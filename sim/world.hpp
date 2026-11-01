#pragma once

#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "sim/drive.hpp"

// Welt der Simulation: fester 1-ms-Schritt, Steuerung und Log auf ganzzahligen ms-Rastern aus sim_params.json.
// Gleicher Seed = byte-gleiche CSV (keine std::*_distribution, kein Aufaddieren von Gleitkommazeit).
namespace frostbyte::sim {

// Skills-Match: 60 s (Spielregel, kein Modellwert).
constexpr int match_ms = 60000;

struct RobotSetup {
    std::string name;            // "stacker" oder "runner"
    std::optional<Pose> start;   // leer, solange field.json null-Koordinaten hat (A-01)
    std::vector<std::string> route;  // Routenzeilen ohne Kommentare; interpretiert erst ab dem Routen-Ticket
    DriveParams drive;               // roboter.<name> aus sim_params.json (SIM-02)
};

struct SimConfig {
    int schritt_ms = 0;
    int steuer_ms = 0;
    int log_ms = 0;
    std::string build = "-";
    RobotSetup stacker;
    RobotSetup runner;
};

struct RunResult {
    int log_rows = 0;       // je Roboter
    int control_ticks = 0;  // je Roboter
};

// splitmix64: auf allen Plattformen gleiche Folge.
class Rng {
public:
    explicit Rng(std::uint64_t seed) : state_(seed) {}
    std::uint64_t next();
    // Gleichverteilt in [0, 1).
    double uniform();

private:
    std::uint64_t state_;
};

// Liest welt und roboter.<name> (Fahrmodell) aus sim_params.json, Startposen aus field.json (posen START_STACKER/START_RUNNER) und die Routen.
SimConfig load_config(const std::string& params_path, const std::string& field_path, const std::string& stacker_route,
                      const std::string& runner_route);

std::vector<std::string> load_route(const std::string& path);

// Ein Match. Schreibt je Roboter eine CSV nach docs/log_format.md (Zeilenende '\n').
RunResult run_match(const SimConfig& cfg, std::uint64_t seed, std::ostream& stacker_csv, std::ostream& runner_csv);

}  // namespace frostbyte::sim
