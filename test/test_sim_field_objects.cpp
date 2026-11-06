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
    const Json params =
        parse_json(R"({"feldobjekte": {"radius_in": {"cup": 1.5, "pin": 1.25}, "roboter_anteil": 0.4}})");
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
    const Json field = parse_json(
        R"({"objekte_start": {"cups": [ {"id":"C1","x_in":1,"y_in":2,"lage":"steht","ebene":0} ], "pins": []}})");
    CHECK(load_field_objects(parse_json(R"({"welt": {}})"), field).objects().empty());
}

TEST_CASE("Objekte: ungültige Params werden abgelehnt") {
    const Json field = parse_json(R"({"objekte_start": {"cups": [], "pins": []}})");
    CHECK_THROWS(load_field_objects(
        parse_json(R"({"feldobjekte": {"radius_in": {"cup": 0, "pin": 1}, "roboter_anteil": 0.5}})"), field));
    CHECK_THROWS(load_field_objects(
        parse_json(R"({"feldobjekte": {"radius_in": {"cup": 1, "pin": 1}, "roboter_anteil": 1.5}})"), field));
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
    for (std::size_t pos = log.find("TOPPLED:CUP_X"); pos != std::string::npos;
         pos = log.find("TOPPLED:CUP_X", pos + 1))
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
    // Dieselbe Endpose prüft der Browser (livesim_test.js, Abschnitt 13). Entstanden mit:
    // frostbyte_sim --input test/fixtures/objects_input.csv --params test/fixtures/sim_params_objects.json
    //   --field test/fixtures/field_objects.json --noise none  (Stacker, Zeile t = 3.000 s: tx_in -18.643).
    // Ändert sich das Modell bewusst, beide Zahlen zusammen neu erzeugen.
    CHECK(last_x(log) == doctest::Approx(-18.643).epsilon(0.0001));
}

// ---- SIM-34: Aufnahme vom Boden ----

namespace {

RouteControl route_control() {
    RouteControl c;
    c.motion.max_volts = 11.5;
    c.motion.track_width_in = 12.0;
    c.motion.follow_max_volts = 8.0;
    c.lateral.kp = 1.0;
    c.lateral.kd = 0.08;
    c.lateral.out_max_v = 12.0;
    c.angular.kp = 0.15;
    c.angular.ki = 0.5;
    c.angular.kd = 0.01;
    c.angular.i_zone = 10.0;
    c.angular.i_max_v = 2.0;
    c.angular.out_max_v = 12.0;
    return c;
}

ObjectField pickup_field(std::vector<FieldObject> list) {
    ObjectField f(std::move(list), 0.25);
    PickupParams p;
    p.reach_in = 3.0;
    p.fallen = PickMod{1.5, 0.5};
    f.set_pickup(p);
    return f;
}

}  // namespace

TEST_CASE("Aufnahme: ohne Block aufnahme wird nichts gefunden") {
    ObjectField f({cup("C1", 6.0, 0.0)}, 0.25);
    CHECK(f.nearest_pickup(robot_at(0.0)) == -1);
}

TEST_CASE("Aufnahme: nur Objekte in Reichweite des Umrisses, das nächste zum Roboterzentrum gewinnt") {
    // Roboter 10 x 10 bei 0: Vorderkante 5, Reichweite 3 -> Objekte bis Kreisrand x = 8 sind aufnehmbar.
    ObjectField f = pickup_field({cup("FAR", 12.0, 0.0), cup("B", 9.5, 3.0), cup("A", 9.5, -1.0)});
    CHECK(f.nearest_pickup(robot_at(0.0)) == 2);  // A: Kreis ab 7.5 in Reichweite und näher als B
    ObjectField none = pickup_field({cup("FAR", 12.0, 0.0)});
    CHECK(none.nearest_pickup(robot_at(0.0)) == -1);
}

TEST_CASE("Aufnahme: bei gleichem Abstand gewinnt das erste Objekt der Liste") {
    ObjectField f = pickup_field({cup("A", 9.0, 1.0), cup("B", 9.0, -1.0)});
    CHECK(f.nearest_pickup(robot_at(0.0)) == 0);
}

TEST_CASE("Aufnahme: nur ein vom Roboter umgestoßenes Objekt bekommt den Zuschlag") {
    ObjectField f =
        pickup_field({cup("STEHT", 9.0, 5.0), cup("LIEGT_VON_ANFANG", 9.0, -5.0, false), cup("KIPPT", 6.0, 0.0)});
    f.step(robot_at(0.0), 144.0);  // KIPPT wird umgestoßen
    CHECK(f.pick_mod(0).extra_time_s == 0.0);
    CHECK(f.pick_mod(0).extra_fail == 0.0);
    CHECK(f.pick_mod(1).extra_time_s == 0.0);  // Pin am Boden liegt normalerweise: kein Zuschlag
    CHECK(f.pick_mod(2).extra_time_s == doctest::Approx(1.5));
    CHECK(f.pick_mod(2).extra_fail == doctest::Approx(0.5));
    CHECK(f.pick_mod(-1).extra_fail == 0.0);
}

TEST_CASE("Aufnahme: ein aufgenommenes Objekt stört die Kollision nicht mehr und wird nicht noch einmal gefunden") {
    ObjectField f = pickup_field({cup("C1", 6.0, 0.0)});
    const int i = f.nearest_pickup(robot_at(0.0));
    REQUIRE(i == 0);
    f.take(i);
    const ObjectContact c = f.step(robot_at(0.0), 144.0);  // würde sonst kippen
    CHECK(c.toppled.empty());
    CHECK(c.robot_shift.x_in == 0.0);
    CHECK(f.nearest_pickup(robot_at(0.0)) == -1);
    f.take(7);  // ungültiger Index: keine Wirkung
}

TEST_CASE("Aufnahme: load_field_objects liest aufnahme, ungültige Zahlen werden abgelehnt") {
    const Json field = parse_json(R"({"objekte_start": {"cups": [], "pins": []}})");
    const ObjectField f =
        load_field_objects(parse_json(R"({"feldobjekte": {"radius_in": {"cup": 1, "pin": 1}, "roboter_anteil": 0.5,
            "aufnahme": {"reichweite_in": 2.5, "liegt_zusatz_dauer_s": 1.0, "liegt_zusatz_fehlschlag": 0.25}}})"),
                           field);
    CHECK(f.pickup().reach_in == doctest::Approx(2.5));
    CHECK(f.pickup().fallen.extra_time_s == doctest::Approx(1.0));
    CHECK(f.pickup().fallen.extra_fail == doctest::Approx(0.25));
    CHECK(load_field_objects(
              parse_json(R"({"feldobjekte": {"radius_in": {"cup": 1, "pin": 1}, "roboter_anteil": 0.5}})"), field)
              .pickup()
              .reach_in < 0.0);
    CHECK_THROWS(
        load_field_objects(parse_json(R"({"feldobjekte": {"radius_in": {"cup": 1, "pin": 1}, "roboter_anteil": 0.5,
            "aufnahme": {"reichweite_in": -1, "liegt_zusatz_dauer_s": 1.0, "liegt_zusatz_fehlschlag": 0.25}}})"),
                           field));
    CHECK_THROWS(
        load_field_objects(parse_json(R"({"feldobjekte": {"radius_in": {"cup": 1, "pin": 1}, "roboter_anteil": 0.5,
            "aufnahme": {"reichweite_in": 1, "liegt_zusatz_dauer_s": 1.0, "liegt_zusatz_fehlschlag": 1.5}}})"),
                           field));
}

TEST_CASE(
    "Aufnahme: Zuschlag verlängert die Aufnahme und erhöht den Fehlschlag, die Zahl der Ziehungen bleibt gleich") {
    EventParams e;
    e.floor = {2.0, 0.5, 0.0};  // Streuung: die Dauer hängt von der Ziehung ab, Fehlschlag 0
    e.yellow_up_floor = 1.0;
    e.grasp_success = 0.5;
    ObjectModel plain(e, 5);
    ObjectModel modded(e, 5);
    const PickResult a = plain.pick(Source::floor);
    const PickResult b = modded.pick(Source::floor, PickMod{1.5, 0.0});
    CHECK(b.time_s == doctest::Approx(a.time_s + 1.5));
    CHECK(a.success);
    CHECK(b.success);
    // Gleiche Ziehungen: die Folge danach (Dauer der nächsten Aufnahme, dann Greifen) bleibt gleich.
    const PickResult a2 = plain.pick(Source::floor);
    const PickResult b2 = modded.pick(Source::floor, PickMod{1.5, 0.0});
    CHECK(b2.time_s == doctest::Approx(a2.time_s + 1.5));
    for (int i = 0; i < 20; ++i)
        CHECK(plain.grasp() == modded.grasp());
    ObjectModel sure_fail(e, 5);
    CHECK_FALSE(sure_fail.pick(Source::floor, PickMod{0.0, 1.0}).success);
}

TEST_CASE("Aufnahme im Lauf: die Route nimmt den Cup in Reichweite, der Cup verschwindet aus dem Feld") {
    const std::string f = FB_TEST_FIXTURES;
    SimConfig cfg = load_config(f + "/sim_params_game_objects.json", f + "/field_game_objects.json",
                                f + "/game_floor_intake.route", f + "/empty.route", "none");
    std::ostringstream s, r;
    const RunResult res = run_match(cfg, 1, s, r);
    CHECK(res.object_picks == 1);
    CHECK(res.object_topples == 0);  // der Roboter stand still: nichts umgestoßen
}

TEST_CASE("Aufnahme im Lauf: Fehlschlag lässt den Cup liegen") {
    const std::string f = FB_TEST_FIXTURES;
    SimConfig cfg = load_config(f + "/sim_params_game_objects_pickfail.json", f + "/field_game_objects.json",
                                f + "/game_floor_intake.route", f + "/empty.route", "none");
    std::ostringstream s, r;
    CHECK(run_match(cfg, 1, s, r).object_picks == 0);
}

TEST_CASE("Aufnahme im Lauf: ohne Block aufnahme bleibt es beim Verhalten von SIM-22, Logs gleich") {
    const std::string f = FB_TEST_FIXTURES;
    // Referenz: Params ganz ohne feldobjekte. Vergleich: feldobjekte (Körper) vorhanden, aber ohne aufnahme.
    SimConfig base = load_config(f + "/sim_params_game.json", f + "/field_game_objects.json",
                                 f + "/game_floor_intake.route", f + "/empty.route", "none");
    std::ostringstream bs, br;
    const RunResult b = run_match(base, 1, bs, br);
    SimConfig no_reach = load_config(f + "/sim_params_game_objects_noreach.json", f + "/field_game_objects.json",
                                     f + "/game_floor_intake.route", f + "/empty.route", "none");
    std::ostringstream ns, nr;
    const RunResult n = run_match(no_reach, 1, ns, nr);
    CHECK(b.object_picks == 0);
    CHECK(n.object_picks == 0);
    CHECK(bs.str() == ns.str());
    CHECK(br.str() == nr.str());
}

TEST_CASE("Aufnahme im Lauf: ein umgestoßener Cup lässt sich nur mit dem Zuschlag schlechter aufnehmen") {
    const std::string f = FB_TEST_FIXTURES;
    const auto run_with = [&](const char* params) {
        SimConfig cfg = load_config(f + "/" + params, f + "/field_game_objects.json",
                                    f + "/game_floor_topple_intake.route", f + "/empty.route", "none");
        cfg.stacker.control = route_control();
        std::ostringstream s, r;
        return run_match(cfg, 1, s, r);
    };
    const RunResult free_pick = run_with("sim_params_game_objects_fallen_free.json");
    CHECK(free_pick.object_topples == 1);  // die Fahrt stößt den Cup erst um, dann nimmt der Roboter ihn auf
    CHECK(free_pick.object_picks == 1);
    const RunResult sure_fail = run_with("sim_params_game_objects_fallen_sure.json");
    CHECK(sure_fail.object_topples == 1);
    CHECK(sure_fail.object_picks == 0);  // Zuschlag Fehlschlag 1: umgestoßen -> Aufnahme scheitert, Cup bleibt liegen
}
