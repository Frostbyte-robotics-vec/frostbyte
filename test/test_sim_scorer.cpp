#include "doctest/doctest.h"
#include "sim/scorer.hpp"

using namespace frostbyte::sim;

// Erwartungswerte sind die Handrechnungen aus docs/rules_scoring.md (Nummern in den Testnamen).
namespace {
using C = PinColor;
StackItem pin(C lower, C upper) { return StackItem::make_pin(lower, upper); }
StackItem cup(bool opaque_lower) { return StackItem::make_cup(opaque_lower); }
GoalState goal(Zone z, ToggleColor t, std::vector<StackItem> s) { return GoalState{z, t, std::move(s)}; }
}  // namespace

TEST_CASE("Scorer 1: leeres Feld, ein Roboter im Midfield gibt 8") {
    ScoreInput in;
    in.robots_in_midfield = 1;
    const auto r = score_skills(in);
    CHECK(r.total == 8);
    CHECK(r.midfield_robot_points == 8);
    CHECK(r.red_blue_halves == 0);
    CHECK(r.yellow_halves == 0);
}

TEST_CASE("Scorer: leeres Feld ohne Roboter gibt 0") { CHECK(score_skills(ScoreInput{}).total == 0); }

TEST_CASE("Scorer 2: gelbe Hälfte zählt nur, wenn der Toggle des Quadranten die Quadrantenfarbe hat") {
    const auto run = [](ToggleColor t) {
        ScoreInput in;
        in.goals.push_back(goal(Zone::red_quadrant, t, {pin(C::red, C::yellow)}));
        return score_skills(in).total;
    };
    CHECK(run(ToggleColor::red) == 10);
    CHECK(run(ToggleColor::neutral) == 0);
    CHECK(run(ToggleColor::blue) == 0);
}

TEST_CASE("Scorer 2b: untere Hälfte des untersten Pins steckt im Goal und zählt nie") {
    ScoreInput in;
    in.goals.push_back(goal(Zone::midfield, ToggleColor::neutral, {pin(C::red, C::red)}));
    const auto r = score_skills(in);
    CHECK(r.red_blue_halves == 1);  // nur die obere
    CHECK(r.total == 5);
}

TEST_CASE("Scorer 3: Cup mit durchsichtiger unterer Hälfte, falsche Farbe im Quadranten gibt 0") {
    ScoreInput in;
    in.goals.push_back(
        goal(Zone::blue_quadrant, ToggleColor::blue, {pin(C::red, C::blue), cup(false), pin(C::yellow, C::red)}));
    const auto r = score_skills(in);
    CHECK(r.total == 5);
    CHECK(r.red_blue_halves == 1);
    CHECK(r.yellow_halves == 0);
}

TEST_CASE("Scorer 4: Midfield, undurchsichtige untere Cup-Hälfte verdeckt die obere Hälfte von Pin 1") {
    ScoreInput in;
    in.robots_in_midfield = 2;
    in.goals.push_back(
        goal(Zone::midfield, ToggleColor::neutral, {pin(C::yellow, C::red), cup(true), pin(C::blue, C::yellow)}));
    const auto r = score_skills(in);
    CHECK(r.red_blue_halves == 1);
    CHECK(r.yellow_halves == 1);
    CHECK(r.midfield_robot_points == 16);
    CHECK(r.total == 31);
}

TEST_CASE("Scorer 4b: gelb im Midfield ohne Roboter dort gibt 0") {
    ScoreInput in;
    in.goals.push_back(goal(Zone::midfield, ToggleColor::neutral, {pin(C::red, C::yellow)}));
    CHECK(score_skills(in).total == 0);
}

TEST_CASE("Scorer 4c: Cup bestimmt, welche Hälfte sichtbar ist (umgekehrte Ausrichtung)") {
    ScoreInput in;
    in.robots_in_midfield = 1;
    in.goals.push_back(
        goal(Zone::midfield, ToggleColor::neutral, {pin(C::yellow, C::red), cup(false), pin(C::blue, C::yellow)}));
    const auto r = score_skills(in);
    // Pin 1 oben rot sichtbar (5), Pin 2 unten blau verdeckt, Pin 2 oben gelb Owned (10), Roboter 8.
    CHECK(r.total == 5 + 10 + 8);
}

TEST_CASE("Scorer 5: zwei Pins direkt übereinander, der obere ist nicht Placed") {
    ScoreInput in;
    in.goals.push_back(goal(Zone::red_quadrant, ToggleColor::red, {pin(C::blue, C::yellow), pin(C::red, C::red)}));
    CHECK(score_skills(in).total == 10);
}

TEST_CASE("Scorer 6: mehrere Goals addieren, Roboter zählen einmal") {
    ScoreInput in;
    in.robots_in_midfield = 2;
    in.goals.push_back(goal(Zone::red_quadrant, ToggleColor::red, {pin(C::red, C::yellow)}));
    in.goals.push_back(
        goal(Zone::midfield, ToggleColor::neutral, {pin(C::yellow, C::red), cup(true), pin(C::blue, C::yellow)}));
    CHECK(score_skills(in).total == 41);
}

TEST_CASE("Scorer 7: Cup als Erstes ist nicht Placed, nichts darüber zählt") {
    ScoreInput in;
    in.goals.push_back(goal(Zone::midfield, ToggleColor::neutral, {cup(false), pin(C::red, C::red)}));
    CHECK(score_skills(in).total == 0);
}

TEST_CASE("Scorer: zwei Cups hintereinander beenden den gültigen Stapel") {
    ScoreInput in;
    in.goals.push_back(goal(Zone::midfield, ToggleColor::neutral,
                            {pin(C::blue, C::red), cup(false), cup(false), pin(C::red, C::red)}));
    // Pin 1 oben rot sichtbar (untere Cup-Hälfte durchsichtig): 5; alles nach dem zweiten Cup zählt nicht.
    CHECK(score_skills(in).total == 5);
}

TEST_CASE("Scorer: blaue Hälfte im roten Quadranten gibt 0, rote im roten gibt 5") {
    ScoreInput in;
    in.goals.push_back(goal(Zone::red_quadrant, ToggleColor::red, {pin(C::red, C::blue)}));
    CHECK(score_skills(in).total == 0);
    ScoreInput in2;
    in2.goals.push_back(goal(Zone::red_quadrant, ToggleColor::red, {pin(C::blue, C::red)}));
    CHECK(score_skills(in2).total == 5);
}

TEST_CASE("Scorer: ungültige Roboterzahl wird auf 0..2 begrenzt") {
    ScoreInput in;
    in.robots_in_midfield = 5;
    CHECK(score_skills(in).total == 16);
    in.robots_in_midfield = -1;
    CHECK(score_skills(in).total == 0);
}
