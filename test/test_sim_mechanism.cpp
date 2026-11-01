#include <algorithm>
#include <cmath>

#include "doctest/doctest.h"
#include "sim/mechanism.hpp"
#include "sim/objects.hpp"

using namespace frostbyte::sim;

namespace {

const std::string fixtures = FB_TEST_FIXTURES;

constexpr double dt = 0.001;

JointParams test_joint() { return JointParams{-10.0, 90.0, 50.0, 200.0}; }

constexpr int trials = 1000;
// Fertig-Kriterium: Fehlerquote +-2 Prozentpunkte über 1000 Versuche.
constexpr double tolerance = 0.02;

}  // namespace

TEST_CASE("Gelenk: erreicht das Ziel ohne Überschwingen, Geschwindigkeit und Beschleunigung bleiben in den Grenzen") {
    const JointParams p = test_joint();
    Joint j(p, 0.0);
    j.set_target(80.0);
    double max_pos = 0.0;
    double max_vel = 0.0;
    double max_acc = 0.0;
    double last_vel = 0.0;
    for (int i = 0; i < 5000 && !j.at_target(); ++i) {
        j.step(dt);
        max_pos = std::max(max_pos, j.pos());
        max_vel = std::max(max_vel, std::fabs(j.vel()));
        // Der letzte Schritt rastet auf dem Ziel ein (Restgeschwindigkeit wenige Grad/s -> 0), das ist kein Beschleunigen.
        if (!j.at_target())
            max_acc = std::max(max_acc, std::fabs(j.vel() - last_vel) / dt);
        last_vel = j.vel();
    }
    CHECK(j.at_target());
    CHECK(j.pos() == 80.0);
    CHECK(max_pos <= 80.0);
    CHECK(max_vel <= p.max_vel + 1e-9);
    CHECK(max_vel == doctest::Approx(p.max_vel).epsilon(0.01));  // Trapez: Höchstgeschwindigkeit wird erreicht
    CHECK(max_acc <= p.max_acc * 1.05);
}

TEST_CASE("Gelenk: Fahrzeit entspricht dem Trapezprofil") {
    const JointParams p = test_joint();
    Joint j(p, 0.0);
    // 80 Grad: Beschleunigen 0,25 s (6,25 Grad), Bremsen 0,25 s (6,25 Grad), Rest 67,5 Grad mit 50 Grad/s = 1,35 s.
    CHECK(j.time_to_reach(80.0) == doctest::Approx(1.85).epsilon(0.03));
    // Kurze Strecke (Dreiecksprofil): 2 Grad -> 2 * sqrt(2 / 200) = 0,2 s.
    CHECK(j.time_to_reach(2.0) == doctest::Approx(0.2).epsilon(0.05));
}

TEST_CASE("Gelenk: Sollwert außerhalb der Endlagen wird begrenzt, Anschlag hält an") {
    Joint j(test_joint(), 0.0);
    j.set_target(500.0);
    CHECK(j.target() == 90.0);
    for (int i = 0; i < 10000; ++i)
        j.step(dt);
    CHECK(j.pos() == 90.0);
    CHECK(j.at_limit());
    j.set_target(-500.0);
    CHECK(j.target() == -10.0);
}

TEST_CASE("Gelenk: Richtungswechsel mitten in der Fahrt, nie außerhalb der Endlagen") {
    Joint j(test_joint(), 0.0);
    j.set_target(60.0);
    for (int i = 0; i < 400; ++i)
        j.step(dt);
    CHECK(j.vel() > 0.0);
    j.set_target(10.0);
    for (int i = 0; i < 5000; ++i) {
        j.step(dt);
        REQUIRE(j.pos() >= -10.0);
        REQUIRE(j.pos() <= 90.0);
    }
    CHECK(j.pos() == 10.0);
}

TEST_CASE("Greifer: ohne Pin schließt er ganz, mit Pin bleibt er bei der Pinbreite") {
    Claw c(ClawParams{1.5, 0.2, 3.0});
    CHECK(c.width_in() == 1.5);
    c.command_close(true);
    for (int i = 0; i < 2000; ++i)
        c.step(dt);
    CHECK(c.settled());
    CHECK(c.width_in() == doctest::Approx(0.2));
    c.command_open();
    for (int i = 0; i < 2000; ++i)
        c.step(dt);
    CHECK(c.width_in() == doctest::Approx(1.5));
    c.command_close(false);
    for (int i = 0; i < 2000; ++i)
        c.step(dt);
    CHECK(c.width_in() == doctest::Approx(0.0));
}

TEST_CASE("Greifer: Schließzeit aus Weg und Geschwindigkeit") {
    Claw c(ClawParams{1.5, 0.3, 3.0});
    CHECK(c.time_to_settle_after_close(true) == doctest::Approx((1.5 - 0.3) / 3.0).epsilon(0.01));
}

TEST_CASE("Mechanismen: sim_params.json lädt (nur Struktur), Arm nur beim Stacker") {
    const Json j = load_json_file(std::string(FB_ROOT_DIR) + "/sim/sim_params.json");
    CHECK(load_mechanism_params(j, "stacker").has_arm);
    CHECK_FALSE(load_mechanism_params(j, "runner").has_arm);
    CHECK_NOTHROW(load_event_params(j));
    CHECK_THROWS(load_mechanism_params(j, "gamma"));
}

TEST_CASE("Mechanismen: unplausible Grenzen werden abgelehnt") {
    const Json bad = parse_json(
        R"({"roboter":{"x":{"mechanismen":{"lift":{"min_deg":10,"max_deg":0,"max_deg_s":1,"max_deg_s2":1},)"
        R"("claw":{"offen_in":1,"pin_in":0.5,"max_in_s":1}}}}})");
    CHECK_THROWS(load_mechanism_params(bad, "x"));
}

namespace {

EventParams test_events() {
    EventParams p;
    p.loader = {1.0, 0.2, 0.05};
    p.floor = {2.0, 0.5, 0.10};
    p.yellow_up_loader = 0.8;
    p.yellow_up_floor = 0.4;
    p.grasp_success = 0.95;
    p.place_success_level0 = 0.96;
    p.place_penalty_per_level = 0.02;
    return p;
}

double mean(const std::vector<double>& v) {
    double s = 0.0;
    for (double x : v)
        s += x;
    return s / static_cast<double>(v.size());
}

}  // namespace

TEST_CASE("Objekte: Fehlschlagquote der Aufnahme +-2 % über 1000 Versuche (Loader und Boden)") {
    ObjectModel m(test_events(), 1);
    int loader_fail = 0;
    int floor_fail = 0;
    for (int i = 0; i < trials; ++i) {
        loader_fail += m.pick(Source::loader).success ? 0 : 1;
        floor_fail += m.pick(Source::floor).success ? 0 : 1;
    }
    CHECK(static_cast<double>(loader_fail) / trials == doctest::Approx(0.05).epsilon(tolerance / 0.05));
    CHECK(static_cast<double>(floor_fail) / trials == doctest::Approx(0.10).epsilon(tolerance / 0.10));
}

TEST_CASE("Objekte: Dauer der Aufnahme hat Mittel und Streuung wie vorgegeben, nie unter 10 %") {
    ObjectModel m(test_events(), 2);
    std::vector<double> t;
    for (int i = 0; i < trials; ++i)
        t.push_back(m.pick(Source::floor).time_s);
    CHECK(mean(t) == doctest::Approx(2.0).epsilon(0.05));
    double var = 0.0;
    for (double x : t)
        var += (x - mean(t)) * (x - mean(t));
    CHECK(std::sqrt(var / (trials - 1)) == doctest::Approx(0.5).epsilon(0.10));
    CHECK(*std::min_element(t.begin(), t.end()) >= 0.2);
}

TEST_CASE("Objekte: Ausrichtung (gelber Pin oben) trifft Loader und Boden unterschiedlich") {
    ObjectModel m(test_events(), 3);
    int up_loader = 0, ok_loader = 0, up_floor = 0, ok_floor = 0;
    for (int i = 0; i < 4 * trials; ++i) {
        const PickResult a = m.pick(Source::loader);
        const PickResult b = m.pick(Source::floor);
        ok_loader += a.success;
        up_loader += a.yellow_up;
        ok_floor += b.success;
        up_floor += b.yellow_up;
    }
    CHECK(static_cast<double>(up_loader) / ok_loader == doctest::Approx(0.8).epsilon(0.05));
    CHECK(static_cast<double>(up_floor) / ok_floor == doctest::Approx(0.4).epsilon(0.05));
}

TEST_CASE("Objekte: Greiferfehlerquote +-2 % über 1000 Versuche") {
    ObjectModel m(test_events(), 4);
    int fail = 0;
    for (int i = 0; i < trials; ++i)
        fail += m.grasp() ? 0 : 1;
    CHECK(static_cast<double>(fail) / trials == doctest::Approx(0.05).epsilon(tolerance / 0.05));
}

TEST_CASE("Objekte: Setzen wird mit der Ebene unwahrscheinlicher, Fehlerquote +-2 % über 1000 Versuche") {
    ObjectModel m(test_events(), 5);
    CHECK(m.place_success_probability(0) == doctest::Approx(0.96));
    CHECK(m.place_success_probability(3) == doctest::Approx(0.90));
    CHECK(m.place_success_probability(100) == 0.0);
    int fail0 = 0;
    int fail3 = 0;
    for (int i = 0; i < trials; ++i) {
        fail0 += m.place(0) ? 0 : 1;
        fail3 += m.place(3) ? 0 : 1;
    }
    CHECK(static_cast<double>(fail0) / trials == doctest::Approx(0.04).epsilon(tolerance / 0.04));
    CHECK(static_cast<double>(fail3) / trials == doctest::Approx(0.10).epsilon(tolerance / 0.10));
}

TEST_CASE("Zyklus: läuft durch, Fehlerquote gesamt +-2 % über 1000 Versuche, Zeit enthält Aufnahme, Greifer und Lift") {
    const EventParams ep = test_events();
    ObjectModel m(ep, 6);
    const Joint lift(test_joint(), -10.0);
    const Claw claw(ClawParams{1.5, 0.3, 3.0});

    int fail = 0;
    int ok = 0;
    double ok_time = 0.0;
    for (int i = 0; i < trials; ++i) {
        const CycleResult r = m.run_cycle(Source::loader, 1, lift, 60.0, claw);
        if (r.outcome == CycleOutcome::ok) {
            ++ok;
            ok_time += r.time_s;
        } else {
            ++fail;
        }
    }
    const double expected_fail = 1.0 - (1.0 - 0.05) * 0.95 * (0.96 - 0.02);
    CHECK(static_cast<double>(fail) / trials == doctest::Approx(expected_fail).epsilon(tolerance / expected_fail));
    REQUIRE(ok > 0);
    // Mittel: 1,0 s Aufnahme + 0,4 s Greifer + Liftfahrt von -10 nach 60 Grad.
    const double expected_time = 1.0 + 1.2 / 3.0 + Joint(test_joint(), -10.0).time_to_reach(60.0);
    CHECK(ok_time / ok == doctest::Approx(expected_time).epsilon(0.05));
}

TEST_CASE("Zyklus: Fehlschlag beendet ihn an der Stelle, Zeit nur bis dorthin") {
    EventParams ep = test_events();
    ep.loader.fail = 1.0;
    ObjectModel m(ep, 7);
    const Joint lift(test_joint(), -10.0);
    const Claw claw(ClawParams{1.5, 0.3, 3.0});
    const CycleResult r = m.run_cycle(Source::loader, 0, lift, 60.0, claw);
    CHECK(r.outcome == CycleOutcome::pick_failed);
    CHECK(r.time_s < 2.0);

    ep.loader.fail = 0.0;
    ep.grasp_success = 0.0;
    ObjectModel m2(ep, 7);
    CHECK(m2.run_cycle(Source::loader, 0, lift, 60.0, claw).outcome == CycleOutcome::grasp_failed);
    ep.grasp_success = 1.0;
    ep.place_success_level0 = 0.0;
    ObjectModel m3(ep, 7);
    CHECK(m3.run_cycle(Source::loader, 0, lift, 60.0, claw).outcome == CycleOutcome::place_failed);
}

TEST_CASE("Objekte: gleicher Seed = gleiche Folge, anderer Seed = andere") {
    auto run = [](std::uint64_t seed) {
        ObjectModel m(test_events(), seed);
        double acc = 0.0;
        for (int i = 0; i < 50; ++i) {
            const PickResult p = m.pick(Source::floor);
            acc += p.time_s + (m.grasp() ? 1 : 0) + (m.place(2) ? 10 : 0);
        }
        return acc;
    };
    CHECK(run(9) == run(9));
    CHECK(run(9) != run(10));
}

TEST_CASE("Objekte: ungültige Wahrscheinlichkeit wird abgelehnt") {
    const Json bad = parse_json(
        R"({"ereignisse":{"loader_einheit":{"dauer_s":1,"sigma_s":0,"fehlschlag":1.5},)"
        R"("boden_einheit":{"dauer_s":1,"sigma_s":0,"fehlschlag":0},"pin_gelb_oben":{"loader":1,"boden":1},)"
        R"("greifer_fasst":{"erfolg":1},"setzen":{"erfolg_ebene0":1,"abzug_pro_ebene":0}}})");
    CHECK_THROWS(load_event_params(bad));
}
