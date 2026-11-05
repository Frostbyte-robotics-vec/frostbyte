#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "doctest/doctest.h"
#include "sim/intake_model.hpp"
#include "sim/json.hpp"
#include "subsystems/intake.hpp"

using namespace frostbyte;
using namespace frostbyte::sim;

namespace {

// Testwerte, unabhängig von sim/sim_params.json (nur 0 und 1 als Wahrscheinlichkeit, dazu 0.5 für Zufallsfolgen).
IntakeJamParams jam_params(double jam_prob, double clear_prob) {
    IntakeJamParams p;
    p.max_deg_s = 300.0;
    p.jam_prob = jam_prob;
    p.clear_prob = clear_prob;
    return p;
}

IntakeConfig intake_config() {
    IntakeConfig c;
    c.run_v = 10.0;
    c.reverse_v = -8.0;
    c.run_timeout_s = 4.0;
    c.grace_s = 0.3;
    c.stall_deg_s = 20.0;
    c.jam_s = 0.25;
    c.reverse_s = 0.4;
    c.max_retries = 2;
    c.bumper_debounce_s = 0.05;
    return c;
}

// IntakeSubsystem gegen die Rollen der Simulation: Spannung des letzten Takts -> Geschwindigkeit des nächsten.
struct Run {
    IntakeConfig cfg = intake_config();
    IntakeSubsystem intake;
    IntakeRollers rollers;
    double t = 0.0, vel = 0.0;
    bool bumper = false;
    double volts = 0.0;

    Run(const IntakeJamParams& p, std::uint64_t seed) : intake(cfg, 0.0), rollers(p, seed) {}

    void tick() {
        t += 0.01;
        IntakeSensor s;
        s.bumper_pressed = bumper;
        s.velocity_deg_s = vel;
        volts = intake.update(t, s).volts;
        vel = rollers.step(volts);
    }
    void run(double secs) {
        const int n = static_cast<int>(std::lround(secs / 0.01));
        for (int i = 0; i < n; ++i)
            tick();
    }
};

}  // namespace

TEST_CASE("SimIntake: Stau, den der Impuls löst, endet im Lauf bis Bumper") {
    Run r(jam_params(1.0, 1.0), 1);
    r.intake.request_run(r.t);
    r.run(1.0);
    CHECK(r.intake.jam_count() == 1);
    CHECK(r.intake.state() == IntakeState::running);
    CHECK(r.intake.error() == IntakeError::none);
    CHECK_FALSE(r.rollers.jammed());
    CHECK(r.vel > 100.0);  // Rollen laufen wieder
    r.bumper = true;
    r.run(0.2);
    CHECK(r.intake.state() == IntakeState::holding);
    CHECK(r.intake.unit_present());
}

TEST_CASE("SimIntake: hartnäckiger Stau endet in error (jam) mit 0 V") {
    Run r(jam_params(1.0, 0.0), 1);
    r.intake.request_run(r.t);
    r.run(3.0);
    CHECK(r.intake.state() == IntakeState::error);
    CHECK(r.intake.error() == IntakeError::jam);
    CHECK(r.intake.jam_count() == r.cfg.max_retries + 1);  // jeder Impuls scheitert, dann der Fehler
    CHECK(r.volts == 0.0);
    CHECK(r.rollers.jammed());
}

TEST_CASE("SimIntake: ohne Klemmen kein Stau und kein Impuls") {
    Run r(jam_params(0.0, 0.0), 1);
    r.intake.request_run(r.t);
    r.run(1.0);
    CHECK(r.intake.jam_count() == 0);
    CHECK(r.intake.state() == IntakeState::running);
    CHECK(r.vel > 100.0);
}

TEST_CASE("SimIntake: Stopp und Neustart lösen keinen Stau, nur ein Rückwärtsimpuls") {
    int jammed_seeds = 0;
    for (std::uint64_t seed = 1; seed <= 50; ++seed) {
        IntakeRollers r(jam_params(0.5, 0.0), seed);
        r.step(10.0);
        if (!r.jammed())
            continue;
        ++jammed_seeds;
        r.step(0.0);
        r.step(10.0);
        CHECK_MESSAGE(r.jammed(), "Seed " << seed << ": Neustart hat den Stau gelöst");
    }
    REQUIRE(jammed_seeds > 0);  // der Test sieht den Unterschied nur, wenn Staus vorkommen
    IntakeRollers r(jam_params(1.0, 1.0), 1);
    r.step(10.0);
    REQUIRE(r.jammed());
    r.step(-8.0);  // Impuls löst (Wahrscheinlichkeit 1)
    CHECK_FALSE(r.jammed());
}

TEST_CASE("SimIntake: gleicher Seed gibt gleiche Folge, anderer Seed eine andere") {
    auto outcomes = [](std::uint64_t seed) {
        std::vector<int> v;
        Run r(jam_params(0.5, 0.5), seed);
        for (int i = 0; i < 40; ++i) {
            r.intake.request_run(r.t);
            r.run(3.0);
            v.push_back(static_cast<int>(r.intake.state()) * 10 + r.intake.jam_count());
            r.intake.request_stop(r.t);
            r.run(0.1);
        }
        return v;
    };
    CHECK(outcomes(7) == outcomes(7));
    CHECK(outcomes(7) != outcomes(8));
}

TEST_CASE("SimIntake: Parameter nur aus sim_params, ungültige Werte werden abgelehnt") {
    const std::string ok =
        R"({"ereignisse":{"intake_stau":{"rollen_deg_s":250,"klemmt":0.2,"loest_nach_impuls":0.4}}})";
    const IntakeJamParams p = load_intake_jam_params(parse_json(ok));
    CHECK(p.max_deg_s == doctest::Approx(250.0));
    CHECK(p.jam_prob == doctest::Approx(0.2));
    CHECK(p.clear_prob == doctest::Approx(0.4));
    auto bad = [](const char* body) { return std::string("{\"ereignisse\":{\"intake_stau\":{") + body + "}}}"; };
    CHECK_THROWS_AS(
        load_intake_jam_params(parse_json(bad(R"("rollen_deg_s":250,"klemmt":1.5,"loest_nach_impuls":0.4)"))),
        std::runtime_error);
    CHECK_THROWS_AS(load_intake_jam_params(parse_json(bad(R"("rollen_deg_s":0,"klemmt":0.1,"loest_nach_impuls":0.4)"))),
                    std::runtime_error);
    // Die echte Datei ist ladbar (nur Form, nicht die Platzhalterwerte).
    CHECK_NOTHROW(load_intake_jam_params(load_json_file(std::string(FB_ROOT_DIR) + "/sim/sim_params.json")));
}
