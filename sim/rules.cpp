#include "sim/rules.hpp"

#include <algorithm>

namespace frostbyte::sim {

std::vector<RuleViolation> check_rules(const RobotRuleState& s) {
    std::vector<RuleViolation> v;
    if (s.length_in > max_footprint_in || s.width_in > max_footprint_in)
        v.push_back({"SG2", "Footprint größer als 24 x 24 Zoll"});
    if (s.height_in > max_height_in)
        v.push_back({"SG3", "Höhe über 50 Zoll"});
    if (s.pins > max_pins || s.cups > max_cups)
        v.push_back({"SG6", "mehr als 1 Pin oder 1 Cup im Besitz"});
    return v;
}

bool motors_stopped(const std::vector<double>& commands) {
    return std::all_of(commands.begin(), commands.end(), [](double c) { return c == 0.0; });
}

}  // namespace frostbyte::sim
