#include "sim/scorer.hpp"

#include <algorithm>
#include <cstddef>

namespace frostbyte::sim {

namespace {

bool counts_for_zone(PinColor c, Zone z) {
    if (z == Zone::midfield)
        return true;
    return (c == PinColor::red && z == Zone::red_quadrant) || (c == PinColor::blue && z == Zone::blue_quadrant);
}

bool yellow_owned(const GoalState& g, int robots) {
    switch (g.zone) {
        case Zone::red_quadrant: return g.toggle == ToggleColor::red;
        case Zone::blue_quadrant: return g.toggle == ToggleColor::blue;
        case Zone::midfield: return robots >= 1;
    }
    return false;
}

void score_half(PinColor c, const GoalState& g, int robots, ScoreResult& r) {
    if (c == PinColor::yellow) {
        if (!yellow_owned(g, robots))
            return;
        ++r.yellow_halves;
    } else {
        if (!counts_for_zone(c, g.zone))
            return;
        ++r.red_blue_halves;
    }
}

// Zählt die sichtbaren, wertbaren Hälften der gültigen Placed-Kette eines Stapels (SC2, SC3).
void score_goal(const GoalState& g, int robots, ScoreResult& r) {
    using Kind = StackItem::Kind;
    const auto& s = g.stack;
    for (std::size_t i = 0; i < s.size(); ++i) {
        // Gültige Reihenfolge: Pin, Cup, Pin, Cup, ... (gerade Stellen Pin, ungerade Cup); ab dem ersten Verstoß zählt nichts mehr.
        const Kind want = (i % 2 == 0) ? Kind::pin : Kind::cup;
        if (s[i].kind != want)
            return;
        if (s[i].kind == Kind::cup)
            continue;

        // Untere Hälfte: im Goal (i == 0) unsichtbar, sonst in der oberen Hälfte des Cups darunter.
        if (i > 0) {
            const bool hidden =
                !s[i - 1].opaque_lower;  // obere Cup-Hälfte undurchsichtig, wenn die untere es nicht ist
            if (!hidden)
                score_half(s[i].lower, g, robots, r);
        }
        // Obere Hälfte: sichtbar, außer ein Cup darüber hat eine undurchsichtige untere Hälfte.
        const bool cup_above = i + 1 < s.size() && s[i + 1].kind == Kind::cup;
        const bool hidden = cup_above && s[i + 1].opaque_lower;
        if (!hidden)
            score_half(s[i].upper, g, robots, r);
    }
}

}  // namespace

ScoreResult score_skills(const ScoreInput& in) {
    ScoreResult r;
    const int robots = std::clamp(in.robots_in_midfield, 0, max_scored_robots);
    for (const auto& g : in.goals)
        score_goal(g, robots, r);
    r.red_blue_points = r.red_blue_halves * red_blue_half_points;
    r.yellow_points = r.yellow_halves * yellow_half_points;
    r.midfield_robot_points = robots * midfield_robot_points;
    r.total = r.red_blue_points + r.yellow_points + r.midfield_robot_points;
    return r;
}

}  // namespace frostbyte::sim
