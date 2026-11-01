#pragma once

#include "sim/world.hpp"

// Demo-Szenario (SIM-11): zwei Roboter fahren von Hand gesteuert (Spannungsfolgen, kein Regler) aufeinander zu, schieben
// sich kurz, fahren auseinander und gegen den Feldrand. Es ersetzt weder Routen noch das vermessene Feld: Startposen
// und Fahrfolgen sind Platzhalter zum Ansehen im Viewer, keine Feldwahrheit (CLAUDE.md). Die Logs tragen build=demo.
namespace frostbyte::sim {

void apply_demo(SimConfig& cfg);

}  // namespace frostbyte::sim
