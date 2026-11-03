#include "sim/game.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace frostbyte::sim {

void load_game_params(const Json& params, GameSetup& g) {
    g.events = load_event_params(params);
    g.mech[0] = load_mechanism_params(params, "stacker");
    g.mech[1] = load_mechanism_params(params, "runner");
    const Json& spiel = params.at("spiel");
    g.cup_opaque_lower = spiel.at("cup_undurchsichtig_unten").as_bool();
    const double mid = spiel.at("roboter_im_midfield").as_number();
    if (mid < 0.0 || mid > max_scored_robots || mid != std::floor(mid))
        throw std::runtime_error("sim_params spiel.roboter_im_midfield muss 0, 1 oder 2 sein");
    g.robots_in_midfield = static_cast<int>(mid);
}

Game::Game(const GameSetup& setup, std::uint64_t seed_stacker, std::uint64_t seed_runner, int end_ms)
    : setup_(setup),
      models_{ObjectModel(setup.events, seed_stacker), ObjectModel(setup.events, seed_runner)},
      end_ms_(end_ms),
      goals_(setup.goals),
      started_(setup.cycles.size(), false) {
    for (const GameCycle& c : setup_.cycles)
        if (c.robot < 0 || c.robot > 1 || c.goal < 0 || c.goal >= static_cast<int>(goals_.size()) || c.level < 0 ||
            c.level > game_max_level)
            throw std::runtime_error("Game: Zyklus mit ungültigem Roboter, Goal oder Level");
    rescore();
}

void Game::rescore() {
    ScoreInput in;
    in.goals = goals_;
    in.robots_in_midfield = setup_.robots_in_midfield;
    score_ = score_skills(in).total;
}

void Game::step(int t_ms) {
    bool changed = false;
    for (std::size_t i = 0; i < pending_.size();) {
        const Pending p = pending_[i];
        if (p.done_ms > t_ms) {
            ++i;
            continue;
        }
        pending_.erase(pending_.begin() + static_cast<std::ptrdiff_t>(i));
        if (p.done_ms >= end_ms_)
            continue;  // nach 0:00 setzt niemand mehr etwas ab
        GoalState& g = goals_[static_cast<std::size_t>(p.goal)];
        // SC2: Pin, Cup, Pin, … im Wechsel. Pinfarbe = Farbe des Quadranten, die obere Hälfte gelb, wenn der Pin so lag.
        if (g.stack.size() % 2 == 0) {
            const PinColor own = g.zone == Zone::blue_quadrant ? PinColor::blue : PinColor::red;
            g.stack.push_back(StackItem::make_pin(own, p.yellow_up ? PinColor::yellow : own));
        } else {
            g.stack.push_back(StackItem::make_cup(setup_.cup_opaque_lower));
        }
        ++completed_;
        changed = true;
    }
    for (std::size_t i = 0; i < setup_.cycles.size(); ++i) {
        const GameCycle& c = setup_.cycles[i];
        if (started_[i] || c.t_ms > t_ms)
            continue;
        started_[i] = true;
        if (busy_until_[c.robot] > t_ms) {
            ++skipped_;
            continue;
        }
        const MechanismParams& m = setup_.mech[c.robot];
        const Joint lift(m.lift, m.lift.min);
        const double target = m.lift.min + (m.lift.max - m.lift.min) * c.level / game_max_level;
        const Claw claw(m.claw);
        const CycleResult r = models_[c.robot].run_cycle(c.source, c.level, lift, target, claw);
        const int done_ms = c.t_ms + static_cast<int>(std::ceil(r.time_s * 1000.0));
        busy_until_[c.robot] = done_ms;
        if (r.outcome == CycleOutcome::ok)
            pending_.push_back({done_ms, c.goal, r.yellow_up});
        else
            ++failed_;
    }
    if (changed)
        rescore();
}

}  // namespace frostbyte::sim
