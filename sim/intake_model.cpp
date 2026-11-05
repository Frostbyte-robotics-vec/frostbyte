#include "sim/intake_model.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace frostbyte::sim {

namespace {

double probability(const Json& obj, const char* key) {
    const double v = obj.at(key).as_number();
    if (v < 0.0 || v > 1.0)
        throw std::runtime_error(std::string("sim_params ereignisse.intake_stau: ") + key + " muss in [0, 1] liegen");
    return v;
}

}  // namespace

IntakeJamParams load_intake_jam_params(const Json& params) {
    const Json& j = params.at("ereignisse").at("intake_stau");
    IntakeJamParams p;
    p.max_deg_s = j.at("rollen_deg_s").as_number();
    if (!(p.max_deg_s > 0.0))
        throw std::runtime_error("sim_params ereignisse.intake_stau: rollen_deg_s muss positiv sein");
    p.jam_prob = probability(j, "klemmt");
    p.clear_prob = probability(j, "loest_nach_impuls");
    return p;
}

double IntakeRollers::step(double volts) {
    const double v = std::clamp(volts, -12.0, 12.0);
    if (v > 0.0) {
        // Ein neuer Lauf aus dem Stand zieht immer eine Zahl (feste Zahl Ziehungen), sie wirkt nur, wenn noch nichts klemmt:
        // Stopp und Neustart lösen keinen Stau, nur ein Rückwärtsimpuls. Nach einem Impuls bleibt der Zustand, wie er ihn ließ.
        if (last_ == Last::idle) {
            const bool jams = rng_.uniform() < p_.jam_prob;
            if (!jammed_)
                jammed_ = jams;
        }
        last_ = Last::forward;
        return jammed_ ? 0.0 : p_.max_deg_s * v / 12.0;
    }
    if (v < 0.0) {
        // Ein Impuls (Beginn der Rückwärtsfahrt) zieht genau eine Zahl, auch wenn nichts klemmt.
        if (last_ != Last::reverse) {
            const bool clears = rng_.uniform() < p_.clear_prob;
            if (jammed_ && clears)
                jammed_ = false;
        }
        last_ = Last::reverse;
        return p_.max_deg_s * v / 12.0;  // rückwärts kommt frei
    }
    last_ = Last::idle;
    return 0.0;
}

}  // namespace frostbyte::sim
