#include <algorithm>
#include <cmath>

#include "doctest/doctest.h"
#include "sim/drive.hpp"
#include "subsystems/drive.hpp"

using namespace frostbyte;

namespace {

// Testwerte, unabhängig von config/ und sim_params.json.
DriveConfig test_config() {
    DriveConfig c;
    c.deadband = 0.1;
    c.exponent = 2.0;
    c.max_speed = 1.0;
    c.slow_factor = 0.4;
    c.max_volts = 12.0;
    c.timeout_s = 0.25;
    c.hold_timeout_s = 1.0;
    return c;
}

DriveRequest arcade(double forward, double turn, bool slow = false) {
    DriveRequest r;
    r.mode = DriveMode::arcade;
    r.forward = forward;
    r.turn = turn;
    r.slow = slow;
    return r;
}

}  // namespace

TEST_CASE("Fahrkurve: Totzone, Exponent, Vorzeichen") {
    CHECK(shape_input(0.0, 0.1, 2.0) == 0.0);
    CHECK(shape_input(0.1, 0.1, 2.0) == 0.0);
    CHECK(shape_input(-0.05, 0.1, 2.0) == 0.0);
    // Nach der Totzone neu auf 0..1 gedehnt: (0.55 - 0.1) / 0.9 = 0.5, Exponent 2 -> 0.25.
    CHECK(shape_input(0.55, 0.1, 2.0) == doctest::Approx(0.25));
    CHECK(shape_input(-0.55, 0.1, 2.0) == doctest::Approx(-0.25));
    CHECK(shape_input(1.0, 0.1, 2.0) == doctest::Approx(1.0));
    CHECK(shape_input(1.5, 0.1, 2.0) == doctest::Approx(1.0));  // wird begrenzt
    CHECK(shape_input(0.5, 0.0, 1.0) == doctest::Approx(0.5));  // linear ohne Totzone
}

TEST_CASE("Arcade-Mischung: geradeaus, Drehen, Normieren") {
    const auto c = test_config();
    DriveSubsystem d(c, 0.0);
    DriveOutput out;
    d.request(arcade(1.0, 0.0), 0.0);
    out = d.update(0.0);
    CHECK(out.left_v == doctest::Approx(12.0));
    CHECK(out.right_v == doctest::Approx(12.0));
    CHECK(d.state() == DriveState::driving);

    // Drehen auf der Stelle: links vor, rechts zurück (positive Drehung = gegen den Uhrzeigersinn: rechts vor).
    d.request(arcade(0.0, 1.0), 0.01);
    out = d.update(0.01);
    CHECK(out.left_v == doctest::Approx(-12.0));
    CHECK(out.right_v == doctest::Approx(12.0));

    // Vorwärts 1 + Drehen 1 würde 2 ergeben: wird auf 1 normiert, das Verhältnis bleibt.
    d.request(arcade(1.0, 1.0), 0.02);
    out = d.update(0.02);
    CHECK(out.left_v == doctest::Approx(0.0).epsilon(1e-9));
    CHECK(out.right_v == doctest::Approx(12.0));
}

TEST_CASE("Langsam-Modus und Höchstgeschwindigkeit") {
    auto c = test_config();
    c.max_speed = 0.8;
    DriveSubsystem d(c, 0.0);
    DriveOutput out;
    d.request(arcade(1.0, 0.0), 0.0);
    out = d.update(0.0);
    CHECK(out.left_v == doctest::Approx(12.0 * 0.8));
    d.request(arcade(1.0, 0.0, true), 0.01);
    out = d.update(0.01);
    CHECK(out.left_v == doctest::Approx(12.0 * 0.8 * 0.4));
}

TEST_CASE("Totzone: kleine Eingaben fahren nicht") {
    DriveSubsystem d(test_config(), 0.0);
    DriveOutput out;
    d.request(arcade(0.05, -0.08), 0.0);
    out = d.update(0.0);
    CHECK(out.left_v == 0.0);
    CHECK(out.right_v == 0.0);
}

TEST_CASE("Timeout: ohne neuen Befehl hält der Antrieb an") {
    DriveSubsystem d(test_config(), 0.0);
    DriveOutput out;
    d.request(arcade(1.0, 0.0), 1.0);
    out = d.update(1.1);
    CHECK(d.state() == DriveState::driving);
    CHECK(out.left_v > 0.0);
    out = d.update(1.25);
    CHECK(d.state() == DriveState::idle);
    CHECK(out.left_v == 0.0);
    CHECK(out.right_v == 0.0);
    CHECK_FALSE(out.hold);
}

TEST_CASE("HOLD: Spannung 0 mit Bremse, eigenes Timeout, Arcade löst ihn ab") {
    DriveSubsystem d(test_config(), 0.0);
    DriveOutput out;
    DriveRequest h;
    h.mode = DriveMode::hold;
    d.request(h, 0.0);
    out = d.update(0.0);
    CHECK(d.state() == DriveState::holding);
    CHECK(out.hold);
    CHECK(out.left_v == 0.0);
    out = d.update(0.5);  // länger als timeout_s, aber kürzer als hold_timeout_s
    CHECK(d.state() == DriveState::holding);
    d.request(arcade(1.0, 0.0), 0.6);  // manuelle Eingabe übernimmt sofort
    out = d.update(0.6);
    CHECK(d.state() == DriveState::driving);
    CHECK_FALSE(out.hold);

    d.request(h, 1.0);
    out = d.update(2.0);
    CHECK(d.state() == DriveState::idle);
}

TEST_CASE("Stop-Befehl: sofort Halt, ohne Bremse") {
    DriveSubsystem d(test_config(), 0.0);
    DriveOutput out;
    d.request(arcade(1.0, 0.0), 0.0);
    out = d.update(0.0);
    DriveRequest s;
    s.mode = DriveMode::stop;
    d.request(s, 0.01);
    out = d.update(0.01);
    CHECK(d.state() == DriveState::idle);
    CHECK(out.left_v == 0.0);
}

TEST_CASE("Sim fährt ein Quadrat und kommt am Start an") {
    using namespace frostbyte::sim;
    DriveParams p;
    p.kS_V = 0.6;
    p.kV_V_per_in_s = 0.2;
    p.kA_V_per_in_s2 = 0.04;
    p.spurbreite_in = 12.0;
    p.mu = 1.0;
    p.motor_max_V = 12.0;
    p.akku_V = 12.6;
    p.g_in_s2 = 386.1;
    DriveModel model(p, Pose{0.0, 0.0, 0.0});

    auto cfg = test_config();
    cfg.deadband = 0.0;
    cfg.exponent = 1.0;
    DriveSubsystem d(cfg, 0.0);
    DriveOutput out;

    constexpr double dt = 0.001;
    double t = 0.0;
    auto step = [&](double forward, double turn) {
        d.request(arcade(forward, turn), t);
        const DriveOutput out = d.update(t);
        model.step(dt, out.left_v, out.right_v);
        t += dt;
    };
    auto dist = [&](const Pose& a) { return std::hypot(a.x_in - 0.0, a.y_in - 0.0); };

    for (int side = 0; side < 4; ++side) {
        const Pose start = model.pose();
        while (std::hypot(model.pose().x_in - start.x_in, model.pose().y_in - start.y_in) < 36.0)
            step(0.5, 0.0);
        // Rampe auslaufen lassen.
        for (int i = 0; i < 300; ++i)
            step(0.0, 0.0);
        const double target = std::fmod(90.0 * (side + 1), 360.0);
        auto err = [&] {
            double e = target - model.pose().h_deg;
            while (e > 180.0) e -= 360.0;
            while (e <= -180.0) e += 360.0;
            return e;
        };
        // P-Regler auf die Blickrichtung (nur Test; der echte Regler kommt mit den Bewegungen), 2 s Einschwingen.
        for (int i = 0; i < 2000; ++i)
            step(0.0, std::clamp(0.02 * err(), -0.5, 0.5));
        CHECK(std::fabs(err()) < 6.0);
    }
    // Vier Seiten à 36 Zoll: wieder in der Nähe des Starts (Rutschen und Ausrollen sind erlaubt).
    CHECK(dist(model.pose()) < 15.0);
}
