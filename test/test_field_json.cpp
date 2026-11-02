#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "doctest/doctest.h"
#include "sim/json.hpp"

using frostbyte::sim::Json;
using frostbyte::sim::load_json_file;

namespace {

Json field() { return load_json_file(std::string(FB_ROOT_DIR) + "/field/field.json"); }

const Json& find_id(const Json& list, const std::string& id) {
    for (std::size_t i = 0; i < list.size(); ++i)
        if (list[i].at("id").as_string() == id)
            return list[i];
    FAIL("Punkt fehlt in field.json: " << id);
    return list[0];
}

double wrap_deg(double d) {
    d = std::fmod(d, 360.0);
    return d < 0 ? d + 360.0 : d;
}

}  // namespace

// Echte Messwerte (CAD 276-9250-000); Winkel der Goals und Startposen sind Strategie und dürfen null bleiben.
TEST_CASE("field.json: Goals, Toggles und Loader haben x/y und liegen im Feld") {
    const Json f = field();
    const double half = f.at("feld_in").as_number() / 2.0;
    for (const char* key : {"goals", "toggles", "loader"}) {
        const Json& list = f.at(key);
        REQUIRE(list.size() > 0);
        for (std::size_t i = 0; i < list.size(); ++i) {
            const std::string id = list[i].at("id").as_string();
            INFO(id);
            REQUIRE_FALSE(list[i].at("x_in").is_null());
            REQUIRE_FALSE(list[i].at("y_in").is_null());
            CHECK(std::fabs(list[i].at("x_in").as_number()) <= half);
            CHECK(std::fabs(list[i].at("y_in").as_number()) <= half);
        }
    }
}

TEST_CASE("field.json: Toggles und Loader haben ihren Winkel") {
    const Json f = field();
    for (std::size_t i = 0; i < f.at("toggles").size(); ++i)
        CHECK_FALSE(f.at("toggles")[i].at("wand_deg").is_null());
    for (std::size_t i = 0; i < f.at("loader").size(); ++i)
        CHECK_FALSE(f.at("loader")[i].at("anfahrt_deg").is_null());
}

TEST_CASE("field.json: rot und blau sind punktsymmetrisch zur Feldmitte") {
    const Json f = field();
    const std::vector<std::pair<std::string, std::string>> goals = {{"RED_ALLIANCE_1", "BLUE_ALLIANCE_1"},
                                                                    {"RED_ALLIANCE_2", "BLUE_ALLIANCE_2"},
                                                                    {"NEUTRAL_R1", "NEUTRAL_B1"},
                                                                    {"NEUTRAL_R2", "NEUTRAL_B2"}};
    for (const auto& [r, b] : goals) {
        INFO(r << " / " << b);
        const Json& a = find_id(f.at("goals"), r);
        const Json& c = find_id(f.at("goals"), b);
        CHECK(a.at("x_in").as_number() == doctest::Approx(-c.at("x_in").as_number()).epsilon(1e-9));
        CHECK(a.at("y_in").as_number() == doctest::Approx(-c.at("y_in").as_number()).epsilon(1e-9));
    }
    const std::vector<std::pair<std::string, std::string>> toggles = {{"TOGGLE_RED_1", "TOGGLE_BLUE_1"},
                                                                      {"TOGGLE_RED_2", "TOGGLE_BLUE_2"}};
    for (const auto& [r, b] : toggles) {
        INFO(r << " / " << b);
        const Json& a = find_id(f.at("toggles"), r);
        const Json& c = find_id(f.at("toggles"), b);
        CHECK(a.at("x_in").as_number() == doctest::Approx(-c.at("x_in").as_number()).epsilon(1e-9));
        CHECK(a.at("y_in").as_number() == doctest::Approx(-c.at("y_in").as_number()).epsilon(1e-9));
        CHECK(wrap_deg(a.at("wand_deg").as_number() + 180.0) ==
              doctest::Approx(wrap_deg(c.at("wand_deg").as_number())));
    }
    // Loader: RED_n spiegelt sich punktsymmetrisch auf BLUE_n.
    const std::vector<std::pair<std::string, std::string>> loader = {{"LOADER_RED_1", "LOADER_BLUE_1"},
                                                                     {"LOADER_RED_2", "LOADER_BLUE_2"}};
    for (const auto& [r, b] : loader) {
        INFO(r << " / " << b);
        const Json& a = find_id(f.at("loader"), r);
        const Json& c = find_id(f.at("loader"), b);
        CHECK(a.at("x_in").as_number() == doctest::Approx(-c.at("x_in").as_number()).epsilon(1e-9));
        CHECK(a.at("y_in").as_number() == doctest::Approx(-c.at("y_in").as_number()).epsilon(1e-9));
        CHECK(wrap_deg(a.at("anfahrt_deg").as_number() + 180.0) ==
              doctest::Approx(wrap_deg(c.at("anfahrt_deg").as_number())));
    }
}

TEST_CASE("field.json: Midfield-Goal liegt in der Feldmitte") {
    const Json f = field();
    const Json& m = find_id(f.at("goals"), "MIDFIELD");
    CHECK(m.at("x_in").as_number() == doctest::Approx(0.0));
    CHECK(m.at("y_in").as_number() == doctest::Approx(0.0));
}
