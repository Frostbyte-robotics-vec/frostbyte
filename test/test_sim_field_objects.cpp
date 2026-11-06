// SIM-30: Feldobjekte (Cups/Pins) als Kollisionskörper. Erst umstoßen (ein Ereignis je Objekt), danach schieben.
// Alle Zahlen sind eigene Fixture-Werte, nicht die Platzhalter aus sim/sim_params.json.
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

#include "doctest/doctest.h"
#include "sim/field_objects.hpp"
#include "sim/json.hpp"
#include "sim/world.hpp"

using namespace frostbyte::sim;

namespace {

const std::string fixtures = FB_TEST_FIXTURES;

// Roboter 10 x 10 bei (x, 0), Blick +x: Vorderkante bei x + 5.
Box robot_at(double x_in) { return Box{x_in, 0.0, 10.0, 10.0, 0.0}; }

FieldObject cup(const std::string& id, double x_in, double y_in, bool standing = true) {
    return FieldObject{id, x_in, y_in, 2.0, standing};
}

}  // namespace

TEST_CASE("Objekte: Berührung ohne Überlappung tut nichts") {
    ObjectField f({cup("C1", 7.0, 0.0)}, 0.25);  // Kreis reicht bis x = 5.0, Vorderkante bei 5.0
    const ObjectContact c = f.step(robot_at(0.0), 144.0);
    CHECK(c.robot_shift.x_in == 0.0);
    CHECK(c.robot_shift.y_in == 0.0);
    CHECK(c.toppled.empty());
    CHECK(f.objects()[0].standing);
}

TEST_CASE("Objekte: erste Überlappung kippt das Objekt genau einmal und verschiebt noch nichts") {
    ObjectField f({cup("C1", 6.0, 0.0)}, 0.25);  // 1.0 Überlappung
    const ObjectContact first = f.step(robot_at(0.0), 144.0);
    REQUIRE(first.toppled.size() == 1);
    CHECK(first.toppled[0] == "C1");
    CHECK_FALSE(f.objects()[0].standing);
    CHECK(f.objects()[0].x_in == doctest::Approx(6.0));
    CHECK(first.robot_shift.x_in == 0.0);
}

TEST_CASE("Objekte: weiterfahren schiebt das liegende Objekt, Roboter bekommt den Anteil robot_share zurück") {
    ObjectField f({cup("C1", 6.0, 0.0)}, 0.25);
    f.step(robot_at(0.0), 144.0);                          // kippt
    const ObjectContact c = f.step(robot_at(0.0), 144.0);  // weiter in Überlappung 1.0
    CHECK(c.toppled.empty());
    // Roboter weicht um 0.25 * 1.0 gegen die Fahrtrichtung, das Objekt um 0.75 * 1.0 nach vorn: Überlappung ist weg.
    CHECK(c.robot_shift.x_in == doctest::Approx(-0.25));
    CHECK(f.objects()[0].x_in == doctest::Approx(6.75));
    CHECK(f.objects()[0].y_in == doctest::Approx(0.0));
    const ObjectContact again = f.step(robot_at(-0.25), 144.0);
    CHECK(again.robot_shift.x_in == 0.0);
}

TEST_CASE("Objekte: schon liegendes Objekt (Pin am Boden) wird sofort geschoben, ohne Kipp-Ereignis") {
    ObjectField f({cup("P1", 6.0, 0.0, false)}, 0.5);
    const ObjectContact c = f.step(robot_at(0.0), 144.0);
    CHECK(c.toppled.empty());
    CHECK(c.robot_shift.x_in == doctest::Approx(-0.5));
    CHECK(f.objects()[0].x_in == doctest::Approx(6.5));
}

TEST_CASE("Objekte: Objekte stoßen nicht aneinander, Reihenfolge der Ereignisse ist die der Liste") {
    ObjectField f({cup("B", 6.0, 3.0), cup("A", 6.0, -3.0)}, 0.25);
    const ObjectContact c = f.step(robot_at(0.0), 144.0);
    REQUIRE(c.toppled.size() == 2);
    CHECK(c.toppled[0] == "B");
    CHECK(c.toppled[1] == "A");
}

TEST_CASE("Objekte: Objekt an der Wand wird nicht aus dem Feld geschoben, der Roboter bekommt den Rest") {
    // Feld 20 x 20 (Kanten bei +-10), Objekt r = 2 liegt schon an der Wand bei x = 8.
    ObjectField f({cup("W", 8.0, 0.0, false)}, 0.25);
    const ObjectContact c = f.step(robot_at(2.0), 20.0);  // Vorderkante 7.0, Überlappung 1.0 mit Kreis ab 6.0
    CHECK(f.objects()[0].x_in == doctest::Approx(8.0));
    CHECK(c.robot_shift.x_in == doctest::Approx(-1.0));
}

TEST_CASE("Objekte: load_field_objects nimmt nur Objekte am Boden mit Koordinaten, Radius je Art") {
    const Json params = parse_json(R"({"feldobjekte": {"radius_in": {"cup": 1.5, "pin": 1.25}, "roboter_anteil": 0.4}})");
    const Json field = parse_json(R"({"objekte_start": {
        "cups": [ {"id":"C1","x_in":1,"y_in":2,"lage":"steht","ebene":0}, {"id":"C2","x_in":null,"y_in":null,"lage":"steht","ebene":0} ],
        "pins": [ {"id":"P1","x_in":3,"y_in":4,"lage":"liegt","ebene":0}, {"id":"P2","x_in":5,"y_in":6,"lage":"steht","ebene":1} ] }})");
    const ObjectField f = load_field_objects(params, field);
    REQUIRE(f.objects().size() == 2);
    CHECK(f.objects()[0].id == "C1");
    CHECK(f.objects()[0].r_in == doctest::Approx(1.5));
    CHECK(f.objects()[0].standing);
    CHECK(f.objects()[1].id == "P1");
    CHECK(f.objects()[1].r_in == doctest::Approx(1.25));
    CHECK_FALSE(f.objects()[1].standing);
    CHECK(f.robot_share() == doctest::Approx(0.4));
}

TEST_CASE("Objekte: ohne Block feldobjekte in sim_params gibt es keine Objekte") {
    const Json field = parse_json(R"({"objekte_start": {"cups": [ {"id":"C1","x_in":1,"y_in":2,"lage":"steht","ebene":0} ], "pins": []}})");
    CHECK(load_field_objects(parse_json(R"({"welt": {}})"), field).objects().empty());
}

TEST_CASE("Objekte: ungültige Params werden abgelehnt") {
    const Json field = parse_json(R"({"objekte_start": {"cups": [], "pins": []}})");
    CHECK_THROWS(load_field_objects(parse_json(R"({"feldobjekte": {"radius_in": {"cup": 0, "pin": 1}, "roboter_anteil": 0.5}})"), field));
    CHECK_THROWS(load_field_objects(parse_json(R"({"feldobjekte": {"radius_in": {"cup": 1, "pin": 1}, "roboter_anteil": 1.5}})"), field));
}

TEST_CASE("Integration: Fahrt in einen Cup erzeugt genau ein TOPPLED, schiebt ihn danach und bremst den Roboter") {
    const auto drive = [](int t_ms) { return t_ms < 1500 ? std::make_pair(6.0, 6.0) : std::make_pair(0.0, 0.0); };
    SimConfig cfg = load_config(fixtures + "/sim_params_objects.json", fixtures + "/field_objects.json",
                                fixtures + "/empty.route", fixtures + "/empty.route", "none");
    cfg.stacker.open_loop = drive;
    std::ostringstream s, r;
    const RunResult res = run_match(cfg, 1, s, r);
    const std::string log = s.str();
    std::size_t count = 0;
    for (std::size_t pos = log.find("TOPPLED:CUP_X"); pos != std::string::npos; pos = log.find("TOPPLED:CUP_X", pos + 1))
        ++count;
    CHECK(count == 1);
    CHECK(log.find("TOPPLED:CUP_FAR") == std::string::npos);  // weit weg, nie berührt
    CHECK(res.object_topples == 1);

    // Gegenprobe ohne Objekte (sim_params_min hat keinen Block feldobjekte): gleiche Fahrt kommt weiter.
    SimConfig free_cfg = load_config(fixtures + "/sim_params_min.json", fixtures + "/field_objects.json",
                                     fixtures + "/empty.route", fixtures + "/empty.route", "none");
    free_cfg.stacker.open_loop = drive;
    std::ostringstream fs, fr;
    run_match(free_cfg, 1, fs, fr);
    CHECK(fs.str().find("TOPPLED") == std::string::npos);
    const auto last_x = [](const std::string& csv) {
        constexpr char NL = 10;  // Zeilenende
        const std::size_t end = csv.find_last_not_of(NL);
        const std::size_t begin = csv.rfind(NL, end);
        const std::string line = csv.substr(begin + 1, end - begin);
        std::stringstream ss(line);
        std::string c;
        for (int i = 0; i <= 9; ++i)
            std::getline(ss, c, ',');
        return std::stod(c);  // Spalte tx_in
    };
    CHECK(last_x(log) < last_x(fs.str()) - 1.0);
}
