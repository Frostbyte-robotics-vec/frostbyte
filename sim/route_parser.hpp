#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "match/route.hpp"

// Parser für Routen-Dateien (docs/route_format.md, ADR-023). Nur Laptop (Simulation, Werkzeuge, Tests).
// Prüft Syntax, Schlüssel, Zahlen und dass Namen in field.json existieren und zur Kategorie passen. Löst keine Koordinaten auf
// (Platzhalter-Posen dürfen die Prüfung nicht beeinflussen).
namespace frostbyte::sim {

struct RouteNames {
    std::vector<std::string> goals, loaders, toggles, poses;
};

struct RouteResult {
    std::vector<RouteCmd> cmds;
    std::string error;  // leer = ok; sonst "Zeile N: Meldung"
    int error_line = 0;
    bool ok() const { return error.empty(); }
};

RouteResult parse_route(std::string_view text, const RouteNames& names);

// Liest field.json und füllt die Namenstabelle (goals, loader, toggles, posen).
RouteNames route_names_from_field(const std::string& field_json_path);

// Gleiche Datei als C++-Quelltext für den Roboter (tools/route_export.cpp, tools/route2cpp.py).
std::string route_to_cpp(const std::vector<RouteCmd>& cmds, const std::string& symbol);

}  // namespace frostbyte::sim
