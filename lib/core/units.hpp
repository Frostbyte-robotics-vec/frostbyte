#pragma once

#include "util/units.hpp"

// Intern: Zoll, Grad, Sekunden. Millimeter und Millisekunden nur an den Rändern (Sensoren, Bibliotheken, Log).
namespace frostbyte {

constexpr double mm_to_in(double mm) { return mm / 25.4; }
constexpr double in_to_mm(double in) { return in * 25.4; }
constexpr double ms_to_s(double ms) { return ms / 1000.0; }
constexpr double s_to_ms(double s) { return s * 1000.0; }

}  // namespace frostbyte
