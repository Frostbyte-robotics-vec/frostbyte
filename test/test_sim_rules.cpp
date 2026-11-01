#include <algorithm>
#include <string>
#include <vector>

#include "doctest/doctest.h"
#include "sim/rules.hpp"

using namespace frostbyte::sim;

namespace {
bool has_rule(const std::vector<RuleViolation>& v, const std::string& id) {
    return std::any_of(v.begin(), v.end(), [&](const RuleViolation& r) { return r.rule == id; });
}
RobotRuleState ok_state() {
    RobotRuleState s;
    s.length_in = 24.0;
    s.width_in = 24.0;
    s.height_in = 50.0;
    s.pins = 1;
    s.cups = 1;
    return s;
}
}  // namespace

TEST_CASE("Regelprüfer: erlaubter Zustand ohne Verstoß") { CHECK(check_rules(ok_state()).empty()); }

TEST_CASE("SG2 Footprint: genau 24 x 24 ist erlaubt, darüber nicht") {
    auto s = ok_state();
    CHECK_FALSE(has_rule(check_rules(s), "SG2"));
    s.length_in = 24.01;
    CHECK(has_rule(check_rules(s), "SG2"));
    s = ok_state();
    s.width_in = 24.01;
    CHECK(has_rule(check_rules(s), "SG2"));
}

TEST_CASE("SG3 Höhe: genau 50 Zoll ist erlaubt, darüber nicht") {
    auto s = ok_state();
    CHECK_FALSE(has_rule(check_rules(s), "SG3"));
    s.height_in = 50.01;
    CHECK(has_rule(check_rules(s), "SG3"));
}

TEST_CASE("SG6 Besitz: 1 Pin und 1 Cup erlaubt, ein zweiter Pin oder Cup nicht") {
    auto s = ok_state();
    CHECK_FALSE(has_rule(check_rules(s), "SG6"));
    s.pins = 2;
    CHECK(has_rule(check_rules(s), "SG6"));
    s = ok_state();
    s.cups = 2;
    CHECK(has_rule(check_rules(s), "SG6"));
    s = ok_state();
    s.pins = 0;
    s.cups = 0;
    CHECK_FALSE(has_rule(check_rules(s), "SG6"));
}

TEST_CASE("Regelprüfer: mehrere Verstöße werden alle gemeldet") {
    RobotRuleState s;
    s.length_in = 30.0;
    s.width_in = 20.0;
    s.height_in = 60.0;
    s.pins = 3;
    const auto v = check_rules(s);
    CHECK(v.size() == 3);
    CHECK(has_rule(v, "SG2"));
    CHECK(has_rule(v, "SG3"));
    CHECK(has_rule(v, "SG6"));
}

TEST_CASE("Matchende: bei 0:00 müssen alle Motoren Halt sein") {
    CHECK(motors_stopped({0.0, 0.0, 0.0}));
    CHECK(motors_stopped({}));
    CHECK_FALSE(motors_stopped({0.0, 0.0, 1.0}));
    CHECK_FALSE(motors_stopped({-2.0, 0.0, 0.0}));
}
