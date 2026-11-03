#include "sim/game_state.hpp"

#include <algorithm>
#include <stdexcept>

namespace frostbyte::sim {

namespace {

Zone zone_of(const std::string& quadrant) {
    if (quadrant.rfind("RED_", 0) == 0)
        return Zone::red_quadrant;
    if (quadrant.rfind("BLUE_", 0) == 0)
        return Zone::blue_quadrant;
    if (quadrant == "MIDFIELD")
        return Zone::midfield;
    throw std::runtime_error("Spielzustand: unbekannter Quadrant " + quadrant);
}

void check_robot(int robot) {
    if (robot < 0 || robot > 1)
        throw std::runtime_error("Spielzustand: Roboter muss 0 oder 1 sein");
}

}  // namespace

GameState::GameState(const GameSetup& setup) : specs_(setup.goals) {
    for (const GoalSpec& g : specs_) {
        GoalState s;
        s.zone = zone_of(g.quadrant);
        goals_.push_back(s);
        if (s.zone != Zone::midfield)
            toggles_[g.quadrant] = ToggleColor::neutral;  // nicht gesetzt = neutral (SC4)
    }
    for (const StartItem& s : setup.start)
        goals_[goal_index(s.goal)].stack.push_back(s.item);
    for (std::size_t i = 1; i < setup.events.size(); ++i)
        if (setup.events[i].t_ms < setup.events[i - 1].t_ms)
            throw std::runtime_error("Spielzustand: Ereignisse nicht nach Zeit sortiert");
    for (const GameEvent& e : setup.events) {
        check_robot(e.robot);
        if (e.kind == GameEventKind::set_toggle) {
            if (!toggles_.count(e.target))
                throw std::runtime_error("Spielzustand: unbekannter Quadrant " + e.target);
        } else if (e.kind == GameEventKind::place_pin || e.kind == GameEventKind::place_cup) {
            goal_index(e.target);
        }
    }
}

std::size_t GameState::goal_index(const std::string& id) const {
    for (std::size_t i = 0; i < specs_.size(); ++i)
        if (specs_[i].id == id)
            return i;
    throw std::runtime_error("Spielzustand: unbekanntes Goal " + id);
}

std::vector<std::string> GameState::apply(const GameEvent& e) {
    check_robot(e.robot);
    Hand& h = hand_[e.robot];
    switch (e.kind) {
        case GameEventKind::pick_pin:
            if (h.pins >= max_pins)
                return {"RULE:SG6"};
            h.pins = 1;
            h.pin = StackItem::make_pin(e.lower, e.upper);
            break;
        case GameEventKind::pick_cup:
            if (h.cups >= max_cups)
                return {"RULE:SG6"};
            h.cups = 1;
            h.cup = StackItem::make_cup(e.opaque_lower);
            break;
        case GameEventKind::place_pin:
            if (h.pins == 0)
                throw std::runtime_error("Spielzustand: Pin setzen ohne Pin in der Hand");
            goals_[goal_index(e.target)].stack.push_back(h.pin);
            h.pins = 0;
            break;
        case GameEventKind::place_cup:
            if (h.cups == 0)
                throw std::runtime_error("Spielzustand: Cup setzen ohne Cup in der Hand");
            goals_[goal_index(e.target)].stack.push_back(h.cup);
            h.cups = 0;
            break;
        case GameEventKind::set_toggle: {
            const auto it = toggles_.find(e.target);
            if (it == toggles_.end())
                throw std::runtime_error("Spielzustand: unbekannter Quadrant " + e.target);
            it->second = e.toggle;
            break;
        }
    }
    return {};
}

ScoreInput GameState::input(int robots_in_midfield) const {
    ScoreInput in;
    in.goals = goals_;
    for (std::size_t i = 0; i < goals_.size(); ++i)
        if (goals_[i].zone != Zone::midfield)
            in.goals[i].toggle = toggles_.at(specs_[i].quadrant);
    in.robots_in_midfield = robots_in_midfield;
    return in;
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

GameSetup load_game_setup(const Json& field) {
    GameSetup g;
    const Json* goals = field.find("goals");
    if (!goals)
        return g;
    for (std::size_t i = 0; i < goals->size(); ++i)
        g.goals.push_back({(*goals)[i].at("id").as_string(), (*goals)[i].at("quadrant").as_string()});
    if (const Json* m = field.find("midfield")) {
        g.midfield.present = true;
        g.midfield.side_in = m->at("seite_in").as_number();
        g.midfield.x_in = m->at("mitte_x_in").as_number();
        g.midfield.y_in = m->at("mitte_y_in").as_number();
    }
    // Pins, die schon auf einem Goal liegen (objekte_start.pins[].auf_goal): bisher nur gelbe, Pin(gelb, gelb) im Stapel.
    const Json* objects = field.find("objekte_start");
    const Json* pins = objects ? objects->find("pins") : nullptr;
    if (pins)
        for (std::size_t i = 0; i < pins->size(); ++i) {
            const Json& p = (*pins)[i];
            const Json* on = p.find("auf_goal");
            if (!on || on->is_null())
                continue;
            if (p.at("variante").as_string() != "yellow")
                throw std::runtime_error("field.json: Startpin auf Goal " + on->as_string() + " ist nicht gelb");
            g.start.push_back({on->as_string(), StackItem::make_pin(PinColor::yellow, PinColor::yellow)});
        }
    return g;
}

}  // namespace frostbyte::sim
