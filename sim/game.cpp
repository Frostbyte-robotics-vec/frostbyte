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
    const double toggle_s = spiel.at("toggle_dauer_s").as_number();
    if (!(toggle_s > 0.0))
        throw std::runtime_error("sim_params spiel.toggle_dauer_s: Zahl größer 0 erwartet");
    g.toggle_ms = static_cast<int>(std::lround(toggle_s * 1000.0));
    g.jam = IntakeJamParams{};
    if (params.at("ereignisse").find("intake_stau"))
        g.jam = load_intake_jam_params(params);
    g.toggle_reach_in = spiel.at("toggle_reichweite_in").as_number();
    if (!(g.toggle_reach_in > 0.0))
        throw std::runtime_error("sim_params spiel.toggle_reichweite_in: Zahl größer 0 erwartet");
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
      started_(setup.cycles.size(), false),
      lift_{Joint(setup.mech[0].lift, setup.mech[0].lift.min), Joint(setup.mech[1].lift, setup.mech[1].lift.min)},
      // eigene Ströme (nicht die der ObjectModel): die Ziehungen der Objekte bleiben mit und ohne Stau gleich
      rollers_{IntakeRollers(setup.jam, seed_stacker ^ 0x9e3779b97f4a7c15ULL),
               IntakeRollers(setup.jam, seed_runner ^ 0x9e3779b97f4a7c15ULL)} {
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
    for (int robot = 0; robot < 2; ++robot) {
        std::vector<LiftCmd>& cmds = lift_cmds_[robot];
        std::size_t used = 0;
        while (used < cmds.size() && cmds[used].t_ms <= t_ms)
            lift_[robot].set_target(cmds[used++].target_deg);
        cmds.erase(cmds.begin(), cmds.begin() + static_cast<std::ptrdiff_t>(used));
        if (last_step_ms_ >= 0)
            lift_[robot].step((t_ms - last_step_ms_) / 1000.0);
    }
    last_step_ms_ = t_ms;
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
    for (std::size_t i = 0; i < pending_toggles_.size();) {
        const PendingToggle p = pending_toggles_[i];
        if (p.done_ms > t_ms) {
            ++i;
            continue;
        }
        pending_toggles_.erase(pending_toggles_.begin() + static_cast<std::ptrdiff_t>(i));
        if (p.done_ms >= end_ms_)
            continue;  // nach 0:00 zählt nichts mehr
        const ToggleSpec& spec = setup_.toggles[static_cast<std::size_t>(p.toggle)];
        for (int goal : spec.goals)
            goals_[static_cast<std::size_t>(goal)].toggle = spec.color;
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
    if (r.lift_start_s >= 0.0) {  // Hub (auch bei Setzfehler), danach zurück nach unten
        lift_cmds_[robot].push_back({t_ms + static_cast<int>(std::ceil(r.lift_start_s * 1000.0)), target});
        lift_cmds_[robot].push_back({done_ms, m.lift.min});
    }
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

int Game::toggle_index(const std::string& name) const {
    for (std::size_t i = 0; i < setup_.toggles.size(); ++i)
        if (setup_.toggles[i].name == name)
            return static_cast<int>(i);
    return -1;
}

CycleTicket Game::start_toggle(int t_ms, int robot, int toggle, double robot_x_in, double robot_y_in) {
    if (toggle < 0 || toggle >= static_cast<int>(setup_.toggles.size()) || busy_until_[robot] > t_ms) {
        ++rejected_;
        return {};
    }
    const ToggleSpec& spec = setup_.toggles[static_cast<std::size_t>(toggle)];
    if (std::hypot(robot_x_in - spec.x_in, robot_y_in - spec.y_in) > setup_.toggle_reach_in) {  // zu weit weg (SIM-25)
        ++rejected_;
        return {};
    }
    const int done_ms = t_ms + setup_.toggle_ms;
    busy_until_[robot] = done_ms;
    pending_toggles_.push_back({done_ms, toggle, robot});
    return {true, true, done_ms};
}

CycleTicket Game::start_intake(int t_ms, int robot, Source src, const PickMod& mod) {
    Hand& h = hand_[robot];
    if (busy_until_[robot] > t_ms || h.unit) {
        ++rejected_;
        return {};
    }
    const PickResult p =
        models_[robot].pick(src, mod);  // immer zuerst und unverändert: gleiche Ziehungen mit und ohne Stau
    const bool jam_model = setup_.intake_cfg[robot].run_v > 0.0 && setup_.jam.jam_prob > 0.0;
    const JamRun jam = jam_model ? run_jam(t_ms, robot) : JamRun{};
    const bool ok = p.success && jam.ok;
    const int done_ms = t_ms + jam.delay_ms + (jam.ok ? static_cast<int>(std::ceil(p.time_s * 1000.0)) : 0);
    busy_until_[robot] = done_ms;
    h.unit = ok;
    h.unit_ready_ms = done_ms;
    h.unit_yellow_up = p.yellow_up;
    return {true, ok, done_ms};
}

Game::JamRun Game::run_jam(int t_ms, int robot) {
    const IntakeConfig& cfg = setup_.intake_cfg[robot];
    IntakeRollers& rollers = rollers_[robot];
    rollers.step(0.0);  // jeder Lauf beginnt aus dem Stand
    IntakeSubsystem intake(cfg, 0.0);
    intake.request_run(0.0);
    const auto emit = [&](int rel_ms, const char* token) {
        const int at = t_ms + rel_ms;
        if (at < end_ms_)  // nach 0:00 zählt und loggt nichts mehr
            events_[robot].push_back({at, token});
    };
    constexpr int tick_ms = 10;  // Steuertakt
    double vel = 0.0;
    int jams = 0;
    IntakeState prev = intake.state();
    for (int rel_ms = tick_ms; rel_ms <= static_cast<int>(std::ceil(cfg.run_timeout_s * 1000.0)); rel_ms += tick_ms) {
        const IntakeOutput out = intake.update(rel_ms / 1000.0, IntakeSensor{false, vel, true});
        vel = rollers.step(out.volts);
        if (intake.jam_count() > jams) {
            jams = intake.jam_count();
            emit(rel_ms, "INTAKE:JAM");
        }
        if (intake.state() == IntakeState::reversing && prev != IntakeState::reversing)
            emit(rel_ms, "INTAKE:IMPULSE");
        if (intake.state() == IntakeState::error) {
            emit(rel_ms, "INTAKE:ERROR_JAM");
            rollers.step(0.0);
            return {false, rel_ms};
        }
        if (intake.state() == IntakeState::running && !rollers.jammed()) {  // die Rollen ziehen wieder
            rollers.step(0.0);
            return {true, jams > 0 ? rel_ms : 0};
        }
        prev = intake.state();
    }
    emit(static_cast<int>(std::ceil(cfg.run_timeout_s * 1000.0)), "TIMEOUT:INTAKE");
    rollers.step(0.0);
    return {false, static_cast<int>(std::ceil(cfg.run_timeout_s * 1000.0))};
}

std::vector<std::string> Game::take_events(int robot, int t_ms) {
    std::vector<std::string> out;
    std::vector<TimedEvent>& q = events_[robot];
    std::size_t n = 0;
    while (n < q.size() && q[n].t_ms <= t_ms)
        out.push_back(q[n++].token);
    q.erase(q.begin(), q.begin() + static_cast<std::ptrdiff_t>(n));
    return out;
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
    for (std::size_t i = 0; i < pending_toggles_.size();) {
        if (pending_toggles_[i].robot == robot && pending_toggles_[i].done_ms > t_ms)
            pending_toggles_.erase(pending_toggles_.begin() + static_cast<std::ptrdiff_t>(i));
        else
            ++i;
    }
    lift_cmds_[robot].clear();
    std::erase_if(events_[robot],
                  [t_ms](const TimedEvent& e) { return e.t_ms > t_ms; });  // der Abbruch beendet den Stau
    lift_[robot].set_target(setup_.mech[robot].lift.min);
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
