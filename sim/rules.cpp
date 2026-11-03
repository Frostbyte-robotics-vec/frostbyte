#include "sim/rules.hpp"

#include <algorithm>

namespace frostbyte::sim {

std::vector<RuleViolation> check_rules(const RobotRuleState& s) {
    // Eine Quelle für die Vergleiche: lib/util/rule_guard.hpp (ADR-027); hier nur Bits in Text übersetzen.
    const std::uint8_t bits = rule_violations(s.length_in, s.width_in, s.height_in, s.pins, s.cups);
    std::vector<RuleViolation> v;
    if (bits & kRuleSg2)
        v.push_back({"SG2", "Footprint größer als 24 x 24 Zoll"});
    if (bits & kRuleSg3)
        v.push_back({"SG3", "Höhe über 50 Zoll"});
    if (bits & kRuleSg6)
        v.push_back({"SG6", "mehr als 1 Pin oder 1 Cup im Besitz"});
    return v;
}

bool motors_stopped(const std::vector<double>& commands) {
    return std::all_of(commands.begin(), commands.end(), [](double c) { return c == 0.0; });
}

}  // namespace frostbyte::sim
