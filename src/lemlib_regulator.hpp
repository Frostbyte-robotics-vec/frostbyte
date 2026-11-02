#pragma once

#include <algorithm>

#include "control/pid.hpp"
#include "lemlib/pid.hpp"

// LemLib-Adapter (D-05, ADR-017): lemlib::PID hinter der eigenen Regler-Schnittstelle frostbyte::Regulator.
// Liegt in src/, weil LemLib-Header nur in hal/pros/ und src/ stehen dürfen (ADR-003) und ein Regler nicht in die
// Hardware-Schicht gehört. Nur lemlib::PID, nicht lemlib::Chassis (eigene Tasks, schreibt Motoren selbst).
// Unterschiede zum eigenen Pid: lemlib::PID kennt kein dt, es summiert je Aufruf und differenziert je Aufruf. Darum
// werden ki und kd mit der festen Taktzeit umgerechnet (ki * T, kd / T); das gilt nur bei genau einem Aufruf je Takt.
// Keine Grenze für den I-Anteil (i_max_v wird ignoriert); i_zone wird zu windupRange (LemLib löscht das Integral
// außerhalb, der eigene Pid hält es nur an). Die Ausgabe begrenzt dieser Adapter.
// Am Laptop nicht testbar; Vergleich mit dem eigenen Pid am Roboter in D-07.
namespace frostbyte {

class LemlibRegulator : public Regulator {
public:
    LemlibRegulator(const PidGains& g, double period_s)
        : pid_(static_cast<float>(g.kp), static_cast<float>(g.ki * period_s), static_cast<float>(g.kd / period_s),
               static_cast<float>(g.i_zone), false),
          out_max_v_(g.out_max_v) {}

    double update(double error, double /*dt_s: fester Takt*/) override {
        return std::clamp(static_cast<double>(pid_.update(static_cast<float>(error))), -out_max_v_, out_max_v_);
    }
    void reset() override { pid_.reset(); }

private:
    lemlib::PID pid_;
    double out_max_v_;
};

}  // namespace frostbyte
