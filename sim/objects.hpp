#pragma once

#include <cstdint>
#include <string>

#include "sim/json.hpp"
#include "sim/mechanism.hpp"
#include "sim/rng.hpp"

// Objektmodell der Simulation als Ereignisse (SIM-04): Teile werden nicht physikalisch simuliert, sondern als
// Zufallsereignisse mit Dauer und Erfolgswahrscheinlichkeit (Loader, Boden, Ausrichtung, Greifen, Setzen).
// Alle Zahlen aus sim_params.json (ereignisse); jedes Modell hat einen eigenen Rng, den der Aufrufer mit seed füttert.
namespace frostbyte::sim {

enum class Source { loader, floor };

struct UnitEventParams {
    double duration_s = 0.0;
    double sigma_s = 0.0;
    double fail = 0.0;  // Wahrscheinlichkeit, dass die Aufnahme scheitert
};

struct EventParams {
    UnitEventParams loader;
    UnitEventParams floor;
    double yellow_up_loader = 0.0;  // Wahrscheinlichkeit, dass der gelbe Pin oben liegt (Ausrichtung)
    double yellow_up_floor = 0.0;
    double grasp_success = 0.0;
    double place_success_level0 = 0.0;
    double place_penalty_per_level = 0.0;
};

// Liest ereignisse. Wahrscheinlichkeiten außerhalb [0, 1] oder negative Zeiten: std::runtime_error.
EventParams load_event_params(const Json& params);

struct PickResult {
    bool success = false;
    bool yellow_up = false;  // nur sinnvoll, wenn success
    double time_s = 0.0;
};

enum class CycleOutcome { ok, pick_failed, grasp_failed, place_failed };

struct CycleResult {
    CycleOutcome outcome = CycleOutcome::ok;
    double time_s = 0.0;
    bool yellow_up = false;
};

class ObjectModel {
public:
    ObjectModel(const EventParams& p, std::uint64_t seed) : p_(p), rng_(seed) {}

    // Eine Einheit vom Loader bzw. vom Boden aufnehmen: Dauer gleichverteilt mit Mittel duration_s und Streuung
    // sigma_s (nie unter 10 % der mittleren Dauer), Fehlschlag und Ausrichtung als Zufallsereignis.
    PickResult pick(Source src);
    bool grasp();
    // Setzen auf Ebene level >= 0: Erfolg = erfolg_ebene0 - abzug_pro_ebene * level, begrenzt auf [0, 1].
    bool place(int level);
    double place_success_probability(int level) const;

    // Ganzer Zyklus: aufnehmen, Greifer schließt (mit Pin), Lift fährt zur Zielhöhe, setzen. Zeiten addieren sich;
    // bei einem Fehlschlag endet der Zyklus dort (time_s bis dahin). lift darf nicht null sein.
    CycleResult run_cycle(Source src, int level, const Joint& lift, double lift_target_deg, const Claw& claw);
    // Der Rest eines Zyklus ohne Aufnahme (SIM-22, ADR-032): Greifer schließt, Lift fährt, setzen. time_s startet bei
    // time_before_s (Dauer der Aufnahme davor). run_cycle ruft genau das nach pick() auf: gleiche Ziehungen, gleiche Zeit.
    CycleResult place_unit(bool yellow_up, double time_before_s, int level, const Joint& lift, double lift_target_deg,
                           const Claw& claw);
    // Ausrichtung eines Preloads (kein pick): wie ein Pin vom Loader (Annahme, ADR-032).
    bool preload_yellow_up();

private:
    EventParams p_;
    Rng rng_;
};

}  // namespace frostbyte::sim
