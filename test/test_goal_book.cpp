#include <array>
#include <cmath>
#include <limits>
#include <string>

#include "doctest/doctest.h"
#include "sim/json.hpp"
#include "util/goal_book.hpp"

using namespace frostbyte;
using frostbyte::sim::Json;
using frostbyte::sim::load_json_file;

namespace {

const double kNaN = std::numeric_limits<double>::quiet_NaN();

// Eigene Testwerte, keine Zahlen aus field.json (die Ebenenstufe dort ist noch offen).
std::array<GoalEntry, GoalBook::kGoals> fixture() {
    return {{{"G0", 3.0},
             {"G1", 5.0},
             {"G2", 3.0},
             {"G3", 5.0},
             {"G4", 3.0},
             {"G5", 5.0},
             {"G6", 3.0},
             {"G7", 5.0},
             {"G8", 4.0}}};
}

}  // namespace

TEST_CASE("GoalBook: Ebene 0 ist die Goal-Höhe, jede weitere Ebene addiert die Stufe") {
    GoalBook b;
    REQUIRE(b.set(fixture(), 2.5));
    for (int g = 0; g < GoalBook::kGoals; ++g) {
        const double base = fixture()[g].base_in;
        for (int l = 0; l < GoalBook::kLevels; ++l) {
            INFO("goal " << g << " level " << l);
            REQUIRE(b.place_height_in(g, l).has_value());
            CHECK(*b.place_height_in(g, l) == doctest::Approx(base + 2.5 * l));
        }
    }
}

TEST_CASE("GoalBook: Höhe steigt streng mit der Ebene") {
    GoalBook b;
    REQUIRE(b.set(fixture(), 1.0));
    for (int g = 0; g < GoalBook::kGoals; ++g)
        for (int l = 1; l < GoalBook::kLevels; ++l)
            CHECK(*b.place_height_in(g, l) > *b.place_height_in(g, l - 1));
}

TEST_CASE("GoalBook: Ebenen außerhalb 0..5 und ungültige Goals werden abgelehnt") {
    GoalBook b;
    REQUIRE(b.set(fixture(), 2.0));
    CHECK_FALSE(b.place_height_in(0, -1).has_value());
    CHECK_FALSE(b.place_height_in(0, 6).has_value());
    CHECK_FALSE(b.place_height_in(-1, 0).has_value());
    CHECK_FALSE(b.place_height_in(GoalBook::kGoals, 0).has_value());
    CHECK(b.place_height_in(0, 5).has_value());
}

TEST_CASE("GoalBook: Suche nach Name") {
    GoalBook b;
    REQUIRE(b.set(fixture(), 2.0));
    CHECK(b.index_of("G4") == 4);
    CHECK(b.index_of("gibt_es_nicht") == -1);
    CHECK(*b.place_height_in("G8", 1) == doctest::Approx(6.0));
    CHECK_FALSE(b.place_height_in("gibt_es_nicht", 0).has_value());
}

TEST_CASE("GoalBook: unbekannte Ebenenstufe (NaN) liefert nur Ebene 0, nie einen geratenen Wert") {
    GoalBook b;
    REQUIRE(b.set(fixture(), kNaN));
    CHECK(*b.place_height_in(1, 0) == doctest::Approx(5.0));
    CHECK_FALSE(b.place_height_in(1, 1).has_value());
    CHECK_FALSE(b.place_height_in(1, 5).has_value());
    CHECK_FALSE(b.level_step_known());
}

TEST_CASE("GoalBook: ungültige Daten machen das Buch ungültig und es liefert nichts") {
    auto bad_height = fixture();
    bad_height[3].base_in = kNaN;
    auto dup = fixture();
    dup[2].id = "G0";
    auto zero_height = fixture();
    zero_height[0].base_in = 0.0;
    GoalBook b;
    CHECK_FALSE(b.set(bad_height, 2.0));
    CHECK_FALSE(b.valid());
    CHECK_FALSE(b.place_height_in(0, 0).has_value());
    CHECK_FALSE(b.set(dup, 2.0));
    CHECK_FALSE(b.set(zero_height, 2.0));
    CHECK_FALSE(b.set(fixture(), 0.0));
    CHECK_FALSE(b.set(fixture(), -1.0));
    CHECK_FALSE(b.set(fixture(), std::numeric_limits<double>::infinity()));
    CHECK_FALSE(b.valid());
    REQUIRE(b.set(fixture(), 2.0));  // ein gültiger Aufruf heilt
    CHECK(b.valid());
}

TEST_CASE("GoalBook: ungesetztes Buch liefert nichts") {
    GoalBook b;
    CHECK_FALSE(b.valid());
    CHECK_FALSE(b.place_height_in(0, 0).has_value());
}

// Das Buch lässt sich aus field.json füllen: alle 9 Goals mit ihrer Höhe. Prüft Struktur, keine Messwerte.
TEST_CASE("GoalBook: aus field.json gefüllt enthält es alle 9 Goals") {
    const Json f = load_json_file(std::string(FB_ROOT_DIR) + "/field/field.json");
    const Json& goals = f.at("goals");
    REQUIRE(goals.size() == static_cast<std::size_t>(GoalBook::kGoals));
    std::array<std::string, GoalBook::kGoals> names;
    std::array<GoalEntry, GoalBook::kGoals> entries{};
    for (int i = 0; i < GoalBook::kGoals; ++i) {
        names[i] = goals[i].at("id").as_string();
        entries[i] = {names[i].c_str(), goals[i].at("hoehe_in").as_number()};
    }
    const Json& e = f.at("ebenen");
    CHECK(e.at("anzahl").as_number() == GoalBook::kLevels);
    const double step = e.at("stufe_in").is_null() ? kNaN : e.at("stufe_in").as_number();
    GoalBook b;
    REQUIRE(b.set(entries, step));
    for (int i = 0; i < GoalBook::kGoals; ++i) {
        INFO(names[i]);
        REQUIRE(b.place_height_in(names[i].c_str(), 0).has_value());
        CHECK(*b.place_height_in(names[i].c_str(), 0) == doctest::Approx(goals[i].at("hoehe_in").as_number()));
    }
}
