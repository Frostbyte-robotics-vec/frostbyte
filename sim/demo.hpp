#pragma once

#include <string>

#include "sim/world.hpp"

// Demo-Szenario (SIM-11): zwei Roboter fahren von Hand gesteuert (Spannungsfolgen, kein Regler) aufeinander zu, schieben
// sich kurz, fahren auseinander und gegen den Feldrand. Es ersetzt weder Routen noch das vermessene Feld: Startposen
// und Fahrfolgen sind Platzhalter zum Ansehen im Viewer, keine Feldwahrheit (CLAUDE.md). Die Logs tragen build=demo.
namespace frostbyte::sim {

// params_path leer: nur Fahrt, kein Spielzustand (Spalte score bleibt leer). Mit sim_params.json zusätzlich zwei Goals mit
// Platzhalter-Zyklen: jeder Roboter setzt alle 10 s von der Quelle Loader auf Ebene 0 ab (SIM-20, ADR-026).
void apply_demo(SimConfig& cfg, const std::string& params_path = "");

}  // namespace frostbyte::sim
