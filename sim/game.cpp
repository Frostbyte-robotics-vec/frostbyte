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
}

MidfieldSpec load_midfield(const Json& field) {
    MidfieldSpec m;
    if (const Json* j = field.find("midfield")) {
        m.present = true;
        m.side_in = j->at("seite_in").as_number();
        m.x_in = j->at("mitte_x_in").as_number();
        m.y_in = j->at("mitte_y_in").as_number();
    }
    return m;
}

int robots_in_midfield(const MidfieldSpec& m, const std::vector<Box>& robots) {
    if (!m.present)
        return 0;
    const Box field{m.x_in, m.y_in, m.side_in, m.side_in, 45.0};
    int n = 0;
    for (const Box& r : robots)
        n += penetration(r, field).has_value();
    return std::min(n, max_scored_robots);
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
    for (int n = 0; n <= max_scored_robots;
         ++n) {  // gelb im Midfield-Goal hängt davon ab, ob überhaupt ein Roboter dort steht
        in.robots_in_midfield = n;
        score_[n] = score_skills(in).total;
    }
}

int Game::score(int robots_in_midfield) const { return score_[std::clamp(robots_in_midfield, 0, max_scored_robots)]; }

void Game::step(int t_ms) {
    bool changed = false;
    for (std::size_t i = 0; i < pending_.size();) {
        const Pending p = pending_[i];
        if (p.done_ms > t_ms) {
            ++i;
            continue;
        }
        pending_.erase(pending_.begin() + static_cast<std::ptrdiff_t>(i));
        held_[p.robot] = Held{};
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
        launch(t_ms, c.robot, c.goal, c.level, c.source, false);
    }
    if (changed)
        rescore();
}

int Game::pieces_on(int goal) const {
    std::size_t n = goals_[static_cast<std::size_t>(goal)].stack.size();
    for (const Pending& p : pending_)
        n += p.goal == goal;
    return static_cast<int>(n);
}

CycleTicket Game::launch(int t_ms, int robot, int goal, int level, const std::optional<Source>& pick_src,
                         bool yellow_up_in_hand) {
    const MechanismParams& m = setup_.mech[robot];
    const Joint lift(m.lift, m.lift.min);
    const double target = m.lift.min + (m.lift.max - m.lift.min) * level / game_max_level;
    const Claw claw(m.claw);
    const CycleResult r = pick_src ? models_[robot].run_cycle(*pick_src, level, lift, target, claw)
                                   : models_[robot].place_unit(yellow_up_in_hand, 0.0, level, lift, target, claw);
    const int done_ms = t_ms + static_cast<int>(std::ceil(r.time_s * 1000.0));
    busy_until_[robot] = done_ms;
    if (r.outcome == CycleOutcome::ok) {
        // Pin oder Cup wie beim Ablegen: nach der Stapelfolge, die Teile auf dem Weg zum Goal mitgezählt.
        (pieces_on(goal) % 2 == 0 ? held_[robot].pins : held_[robot].cups) = 1;
        pending_.push_back({done_ms, goal, r.yellow_up, robot});
    } else
        ++failed_;
    return {true, r.outcome == CycleOutcome::ok, done_ms};
}

int Game::goal_index(const std::string& name) const {
    for (std::size_t i = 0; i < setup_.goal_names.size(); ++i)
        if (setup_.goal_names[i] == name)
            return static_cast<int>(i);
    return -1;
}

CycleTicket Game::start_intake(int t_ms, int robot, Source src) {
    Hand& h = hand_[robot];
    if (busy_until_[robot] > t_ms || h.unit) {
        ++rejected_;
        return {};
    }
    const PickResult p = models_[robot].pick(src);
    const int done_ms = t_ms + static_cast<int>(std::ceil(p.time_s * 1000.0));
    busy_until_[robot] = done_ms;
    h.unit = p.success;
    h.unit_ready_ms = done_ms;
    h.unit_yellow_up = p.yellow_up;
    return {true, p.success, done_ms};
}

CycleTicket Game::start_cycle(int t_ms, int robot, int goal, int level, bool preload) {
    Hand& h = hand_[robot];
    if (goal < 0 || goal >= static_cast<int>(goals_.size()) || busy_until_[robot] > t_ms) {
        ++rejected_;
        return {};
    }
    const int on_goal = pieces_on(goal);
    if (on_goal > game_max_level || (level != game_level_auto && level != on_goal)) {
        ++rejected_;
        return {};
    }
    bool yellow_up = false;
    if (h.unit && t_ms >= h.unit_ready_ms) {
        yellow_up = h.unit_yellow_up;
        h.unit = false;
    } else if (!h.unit && preload && !h.preload_used) {
        h.preload_used = true;
        yellow_up = models_[robot].preload_yellow_up();
    } else {
        ++rejected_;
        return {};
    }
    return launch(t_ms, robot, goal, on_goal, std::nullopt, yellow_up);
}

void Game::cancel(int t_ms, int robot) {
    bool dropped = false;
    for (std::size_t i = 0; i < pending_.size();) {
        if (pending_[i].robot == robot && pending_[i].done_ms > t_ms) {
            pending_.erase(pending_.begin() + static_cast<std::ptrdiff_t>(i));
            dropped = true;
        } else
            ++i;
    }
    Hand& h = hand_[robot];
    if (h.unit && h.unit_ready_ms > t_ms) {
        h.unit = false;
        dropped = true;
    }
    if (dropped)
        held_[robot] = Held{};
    busy_until_[robot] = std::min(busy_until_[robot], t_ms);  // auch ein fehlgeschlagener Befehl gibt den Roboter frei
}

}  // namespace frostbyte::sim
