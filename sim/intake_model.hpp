#pragma once

#include <cstdint>

#include "sim/json.hpp"
#include "sim/rng.hpp"

// Rollenmodell des Intakes mit Stau (SIM-28, ADR-037): Die Rollen drehen mit der Spannung; beim Start eines Laufs aus dem
// Stand klemmen sie mit einer Wahrscheinlichkeit. Solange sie klemmen, stehen sie bei Vorwärtsspannung. Ein Rückwärtsimpuls
// löst den Stau mit einer weiteren Wahrscheinlichkeit (0 = nie, 1 = immer). Alle Zahlen aus sim_params.json
// (ereignisse.intake_stau); der Aufrufer füttert den Rng mit seed.
namespace frostbyte::sim {

struct IntakeJamParams {
    double max_deg_s = 0.0;   // Rollengeschwindigkeit bei 12 V
    double jam_prob = 0.0;    // Wahrscheinlichkeit, dass ein Lauf (Start aus dem Stand) klemmt
    double clear_prob = 0.0;  // Wahrscheinlichkeit, dass ein Rückwärtsimpuls den Stau löst
};

// Liest ereignisse.intake_stau. Wahrscheinlichkeiten außerhalb [0, 1] oder max_deg_s <= 0: std::runtime_error.
IntakeJamParams load_intake_jam_params(const Json& params);

class IntakeRollers {
public:
    IntakeRollers(const IntakeJamParams& p, std::uint64_t seed) : p_(p), rng_(seed) {}

    // Spannung des letzten Takts -> Rollengeschwindigkeit in Grad/s (positiv = Einziehen).
    double step(double volts);
    bool jammed() const { return jammed_; }

private:
    enum class Last { idle, forward, reverse };
    IntakeJamParams p_;
    Rng rng_;
    Last last_ = Last::idle;
    bool jammed_ = false;
};

}  // namespace frostbyte::sim
