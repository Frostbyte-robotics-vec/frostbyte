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

TEST_CASE("field.json: Goal-Anfahrt zeigt von der nächsten Wand ins Feld, rot/blau um 180 Grad gedreht") {
    const Json f = field();
    const Json& goals = f.at("goals");
    // Nächste Wand: größerer Betrag von |x| bzw. |y|; die Anfahrtsrichtung zeigt von dort zur Feldmitte (Wand + 180).
    for (std::size_t i = 0; i < goals.size(); ++i) {
        const std::string id = goals[i].at("id").as_string();
        if (id == "MIDFIELD") {
            CHECK(goals[i].at("anfahrt_deg").is_null());
            continue;
        }
        INFO(id);
        REQUIRE_FALSE(goals[i].at("anfahrt_deg").is_null());
        const double x = goals[i].at("x_in").as_number();
        const double y = goals[i].at("y_in").as_number();
        const double wand = std::fabs(x) > std::fabs(y) ? (x < 0 ? 180.0 : 0.0) : (y < 0 ? 270.0 : 90.0);
        CHECK(wrap_deg(goals[i].at("anfahrt_deg").as_number()) == doctest::Approx(wrap_deg(wand + 180.0)));
    }
    const std::vector<std::pair<std::string, std::string>> paare = {{"RED_ALLIANCE_1", "BLUE_ALLIANCE_1"},
                                                                    {"RED_ALLIANCE_2", "BLUE_ALLIANCE_2"},
                                                                    {"NEUTRAL_R1", "NEUTRAL_B1"},
                                                                    {"NEUTRAL_R2", "NEUTRAL_B2"}};
    for (const auto& [r, b] : paare) {
        INFO(r << " / " << b);
        CHECK(wrap_deg(find_id(goals, r).at("anfahrt_deg").as_number() + 180.0) ==
              doctest::Approx(wrap_deg(find_id(goals, b).at("anfahrt_deg").as_number())));
    }
}

namespace {

// Alle Objekte einer Liste an gespiegelter Stelle wiederfinden (Punktsymmetrie), gleiche Variante.
void check_point_symmetric(const Json& list, const std::string& variante_key) {
    for (std::size_t i = 0; i < list.size(); ++i) {
        const double x = list[i].at("x_in").as_number();
        const double y = list[i].at("y_in").as_number();
        const int ebene = static_cast<int>(list[i].at("ebene").as_number());
        const std::string var = variante_key.empty() ? "" : list[i].at(variante_key).as_string();
        bool gefunden = false;
        for (std::size_t j = 0; j < list.size(); ++j) {
            if (std::fabs(list[j].at("x_in").as_number() + x) > 1e-6 ||
                std::fabs(list[j].at("y_in").as_number() + y) > 1e-6)
                continue;
            if (static_cast<int>(list[j].at("ebene").as_number()) != ebene)
                continue;
            if (!variante_key.empty() && list[j].at(variante_key).as_string() != var)
                continue;
            gefunden = true;
        }
        INFO(list[i].at("id").as_string());
        CHECK(gefunden);
    }
}

}  // namespace

TEST_CASE("field.json: Startobjekte - Cups (klar) und Pins (alle anderen, mit Variante)") {
    const Json f = field();
    const Json& cups = f.at("objekte_start").at("cups");
    const Json& pins = f.at("objekte_start").at("pins");
    const double half = f.at("feld_in").as_number() / 2.0;
    CHECK(cups.size() == 36);
    CHECK(pins.size() == 37);

    std::vector<std::string> ids;
    for (const Json* list : {&cups, &pins}) {
        for (std::size_t i = 0; i < list->size(); ++i) {
            const Json& o = (*list)[i];
            ids.push_back(o.at("id").as_string());
            INFO(ids.back());
            CHECK(std::fabs(o.at("x_in").as_number()) <= half);
            CHECK(std::fabs(o.at("y_in").as_number()) <= half);
        }
    }
    for (std::size_t i = 0; i < ids.size(); ++i)
        for (std::size_t j = i + 1; j < ids.size(); ++j)
            CHECK_MESSAGE(ids[i] != ids[j], "doppelte id: " << ids[i]);

    const std::vector<std::string> varianten = {"yellow", "red_yellow", "blue_yellow", "red_blue"};
    int rot_gelb = 0, blau_gelb = 0, gelb = 0, rot_blau = 0;
    for (std::size_t i = 0; i < pins.size(); ++i) {
        const std::string v = pins[i].at("variante").as_string();
        bool bekannt = false;
        for (const auto& k : varianten)
            bekannt = bekannt || k == v;
        INFO(pins[i].at("id").as_string());
        CHECK(bekannt);
        rot_gelb += v == "red_yellow";
        blau_gelb += v == "blue_yellow";
        gelb += v == "yellow";
        rot_blau += v == "red_blue";
        // Liegende Pins sagen, wohin die gelbe Seite zeigt; stehende nicht.
        if (pins[i].at("lage").as_string() == "liegt")
            CHECK_FALSE(pins[i].at("gelb_deg").is_null());
    }
    CHECK(rot_gelb == 8);
    CHECK(blau_gelb == 8);
    CHECK(gelb == 17);
    CHECK(rot_blau == 4);

    check_point_symmetric(cups, "");
    check_point_symmetric(pins, "");
}

TEST_CASE("field.json: Pins drehen sich mit der Feldmitte um 180 Grad (Farbe vertauscht, gelbe Seite gedreht)") {
    const Json f = field();
    const Json& pins = f.at("objekte_start").at("pins");
    auto tausch = [](const std::string& v) {
        return v == "red_yellow" ? std::string("blue_yellow") : v == "blue_yellow" ? std::string("red_yellow") : v;
    };
    for (std::size_t i = 0; i < pins.size(); ++i) {
        const Json& a = pins[i];
        bool gefunden = false;
        for (std::size_t j = 0; j < pins.size(); ++j) {
            const Json& b = pins[j];
            if (std::fabs(a.at("x_in").as_number() + b.at("x_in").as_number()) > 1e-6 ||
                std::fabs(a.at("y_in").as_number() + b.at("y_in").as_number()) > 1e-6 ||
                a.at("ebene").as_number() != b.at("ebene").as_number() ||
                b.at("variante").as_string() != tausch(a.at("variante").as_string()))
                continue;
            if (!a.at("gelb_deg").is_null() && wrap_deg(a.at("gelb_deg").as_number() + 180.0) !=
                                                   doctest::Approx(wrap_deg(b.at("gelb_deg").as_number())))
                continue;
            if (a.at("variante").as_string() == "red_blue") {
                const Json* oa = a.find("oben_farbe");
                const Json* ob = b.find("oben_farbe");
                if (!oa || !ob || oa->as_string() == ob->as_string())
                    continue;
            }
            gefunden = true;
        }
        INFO(a.at("id").as_string());
        CHECK(gefunden);
    }
}

namespace {

struct Start {
    double x, y, h;
};

Start start_of(const Json& f, const std::string& id) {
    const Json& p = find_id(f.at("posen"), id);
    REQUIRE_FALSE(p.at("x_in").is_null());
    REQUIRE_FALSE(p.at("y_in").is_null());
    REQUIRE_FALSE(p.at("h_deg").is_null());
    return {p.at("x_in").as_number(), p.at("y_in").as_number(), p.at("h_deg").as_number()};
}

// Dreieckiges Quadrant = Wand, zu der der Mittelpunkt gehört (Diagonalen trennen): 0 = +x, 1 = +y, 2 = -x, 3 = -y.
int wand_of(const Start& s) {
    if (std::fabs(s.x) >= std::fabs(s.y))
        return s.x > 0 ? 0 : 2;
    return s.y > 0 ? 1 : 3;
}

// Fest eingebaute Footprints (CLAUDE.md): Stacker 24 in (SG2), Runner 15 in. Pose steht mit dem Rücken an der Wand.
void check_start(const Json& f, const std::string& id, double seite_in) {
    INFO(id);
    const Start s = start_of(f, id);
    const double half = f.at("feld_in").as_number() / 2.0;
    const int wand = wand_of(s);
    const double abstand = wand % 2 == 0 ? half - std::fabs(s.x) : half - std::fabs(s.y);
    CHECK(abstand == doctest::Approx(seite_in / 2.0));  // Rückseite berührt die Wand
    CHECK(std::fabs(s.x) + seite_in / 2.0 <= half + 1e-9);
    CHECK(std::fabs(s.y) + seite_in / 2.0 <= half + 1e-9);
    // Blick senkrecht von der Wand weg, ins Feld.
    const double ins_feld = wrap_deg(wand * 90.0 + 180.0);
    CHECK(wrap_deg(s.h) == doctest::Approx(ins_feld));
}

}  // namespace

TEST_CASE("field.json: Startposen an der Wand, ins Feld gerichtet, verschiedene Dreiecke (Quadrant Placement)") {
    const Json f = field();
    check_start(f, "START_STACKER", 24.0);
    check_start(f, "START_RUNNER", 15.0);
    CHECK(wand_of(start_of(f, "START_STACKER")) != wand_of(start_of(f, "START_RUNNER")));
}
