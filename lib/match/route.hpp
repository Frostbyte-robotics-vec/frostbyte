#pragma once

#include <cstdint>

// Befehl einer Route (docs/route_format.md), flach und ohne dynamischen Speicher: ein Block (parallel, repeat) und `at`
// stehen als eigener Befehl, die folgenden `body_len` Befehle (verschachtelte mitgezählt) gehören dazu. So kann der
// RoutePlayer (A-03) eine feste Liste auf dem Roboter ablaufen. Der Parser (sim/route_parser.hpp) läuft nur auf dem Laptop.
// Namen werden hier nur gespeichert; Koordinaten löst erst der Player aus field.json auf (ADR-023).
namespace frostbyte {

enum class RouteOp : std::uint8_t {
    target_p10,  // value = Punkte
    start,       // name (Pose) oder has_pose mit x/y/h
    goto_name,   // name, tol_in, timeout_s
    goto_pose,   // x/y/h, timeout_s
    turn,        // h_deg = absolute Richtung im Feldrahmen
    intake,      // until=unit
    cycle,       // name = Goal, level (-1 = auto), source
    toggle,      // name
    signal,      // name
    wait_for,    // robot + name
    wait,        // value = Sekunden
    parallel,    // body_len
    repeat,      // value = n, body_len
    at,          // value = Matchzeit in s, body_len = 1
    end_hold,
};

enum class RouteOnTimeout : std::uint8_t { skip, retry, abort };
enum class RouteSource : std::uint8_t { preload, quiver };
enum class RouteRobot : std::uint8_t { none, stacker, runner };

constexpr int kRouteNameLen = 32;
constexpr int kRouteLevelAuto = -1;
constexpr int kRouteMaxLevel = 5;  // Ebenen 0..5, siehe ADR-022

struct RouteCmd {
    RouteOp op = RouteOp::end_hold;
    int line = 0;  // Zeile in der Quelldatei, für Meldungen
    char name[kRouteNameLen] = {};
    bool has_pose = false;
    double x_in = 0.0, y_in = 0.0, h_deg = 0.0;
    double value = 0.0;
    double tol_in = -1.0;     // < 0: Standard
    double timeout_s = -1.0;  // < 0: kein eigenes Timeout
    int level = kRouteLevelAuto;
    int body_len = 0;
    RouteSource source = RouteSource::preload;
    RouteOnTimeout on_timeout = RouteOnTimeout::skip;
    RouteRobot robot = RouteRobot::none;
};

}  // namespace frostbyte
