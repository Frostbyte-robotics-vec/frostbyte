#include "sim/objects.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace frostbyte::sim {

namespace {

double probability(const Json& obj, const char* key) {
    const double v = obj.at(key).as_number();
    if (v < 0.0 || v > 1.0)
        throw std::runtime_error(std::string("sim_params ereignisse: ") + key + " muss in [0, 1] liegen");
    return v;
}

double non_negative(const Json& obj, const char* key) {
    const double v = obj.at(key).as_number();
    if (v < 0.0)
        throw std::runtime_error(std::string("sim_params ereignisse: ") + key + " darf nicht negativ sein");
    return v;
}

UnitEventParams unit_from_json(const Json& j) {
    UnitEventParams u;
    u.duration_s = non_negative(j, "dauer_s");
    u.sigma_s = non_negative(j, "sigma_s");
    u.fail = probability(j, "fehlschlag");
    return u;
}

}  // namespace

EventParams load_event_params(const Json& params) {
    const Json& e = params.at("ereignisse");
    EventParams p;
    p.loader = unit_from_json(e.at("loader_einheit"));
    p.floor = unit_from_json(e.at("boden_einheit"));
    p.yellow_up_loader = probability(e.at("pin_gelb_oben"), "loader");
    p.yellow_up_floor = probability(e.at("pin_gelb_oben"), "boden");
    p.grasp_success = probability(e.at("greifer_fasst"), "erfolg");
    p.place_success_level0 = probability(e.at("setzen"), "erfolg_ebene0");
    p.place_penalty_per_level = non_negative(e.at("setzen"), "abzug_pro_ebene");
    return p;
}

PickResult ObjectModel::pick(Source src, const PickMod& mod) {
    const UnitEventParams& u = src == Source::loader ? p_.loader : p_.floor;
    const double yellow_up = src == Source::loader ? p_.yellow_up_loader : p_.yellow_up_floor;

    // Gleichverteilung mit Mittel duration_s und Standardabweichung sigma_s (Halbbreite sigma * sqrt(3)).
    const double spread = u.sigma_s * std::sqrt(3.0) * (2.0 * rng_.uniform() - 1.0);
    PickResult r;
    r.time_s = std::max(u.duration_s + spread, 0.1 * u.duration_s) + mod.extra_time_s;
    // Feste Zahl Ziehungen je Aufruf: ein Fehlschlag verschiebt die Folge der übrigen nicht.
    const double fail_draw = rng_.uniform();
    const double yellow_draw = rng_.uniform();
    r.success = fail_draw >= std::min(u.fail + mod.extra_fail, 1.0);
    r.yellow_up = r.success && yellow_draw < yellow_up;
    return r;
}

bool ObjectModel::grasp() { return rng_.uniform() < p_.grasp_success; }

double ObjectModel::place_success_probability(int level) const {
    const double v = p_.place_success_level0 - p_.place_penalty_per_level * std::max(level, 0);
    return std::min(std::max(v, 0.0), 1.0);
}

bool ObjectModel::place(int level) { return rng_.uniform() < place_success_probability(level); }

CycleResult ObjectModel::run_cycle(Source src, int level, const Joint& lift, double lift_target_deg, const Claw& claw) {
    const PickResult pick_r = pick(src);
    if (!pick_r.success) {
        CycleResult r;
        r.time_s = pick_r.time_s;
        r.outcome = CycleOutcome::pick_failed;
        return r;
    }
    return place_unit(pick_r.yellow_up, pick_r.time_s, level, lift, lift_target_deg, claw);
}

CycleResult ObjectModel::place_unit(bool yellow_up, double time_before_s, int level, const Joint& lift,
                                    double lift_target_deg, const Claw& claw) {
    CycleResult r;
    r.time_s = time_before_s;
    r.yellow_up = yellow_up;

    r.time_s += claw.time_to_settle_after_close(true);
    if (!grasp()) {
        r.outcome = CycleOutcome::grasp_failed;
        return r;
    }

    r.lift_start_s = r.time_s;
    r.time_s += lift.time_to_reach(lift_target_deg);
    if (!place(level))
        r.outcome = CycleOutcome::place_failed;
    return r;
}

bool ObjectModel::preload_yellow_up() { return rng_.uniform() < p_.yellow_up_loader; }

}  // namespace frostbyte::sim
