#include <cmath>
#include <limits>

#include "config/runner.hpp"
#include "config/stacker.hpp"
#include "doctest/doctest.h"
#include "subsystems/intake.hpp"

using namespace frostbyte;

namespace {

// Testwerte, unabhaengig von config/.
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

// Rollen drehen mit run_v, wenn nichts klemmt; klemmt es, stehen sie. Der Bumper wird von aussen gesetzt.
struct Bench {
    IntakeConfig cfg = intake_config();
    IntakeSubsystem intake{cfg, 0.0};
    double t = 0.0;
    bool jammed = false;  // Rollen blockiert, solange vorwaerts gefahren wird
    bool bumper = false;
    bool ok = true;
    double vel = 0.0;
    IntakeOutput out{};

    void tick() {
        t += 0.01;
        IntakeSensor s;
        s.bumper_pressed = bumper;
        s.ok = ok;
        s.velocity_deg_s = vel;
        out = intake.update(t, s);
        // Strecke: die Spannung des letzten Takts bestimmt die Geschwindigkeit des naechsten.
        if (out.volts > 0.0)
            vel = jammed ? 0.0 : 300.0;
        else if (out.volts < 0.0)
            vel = -300.0;  // rueckwaerts kommt frei
        else
            vel = 0.0;
    }
    void run(double secs) {
        const int n = static_cast<int>(std::lround(secs / 0.01));
        for (int i = 0; i < n; ++i)
            tick();
    }
};

}  // namespace

TEST_CASE("Intake: ohne Befehl keine Spannung") {
    Bench b;
    b.run(1.0);
    CHECK(b.intake.state() == IntakeState::idle);
    CHECK(b.out.volts == 0.0);
}

TEST_CASE("Intake: Lauf bis Bumper, dann Halt mit Einheit") {
    Bench b;
    REQUIRE(b.intake.request_run(b.t));
    b.run(0.5);
    CHECK(b.intake.state() == IntakeState::running);
    CHECK(b.out.volts == doctest::Approx(10.0));
    b.bumper = true;
    b.run(0.03);  // kuerzer als die Entprellzeit
    CHECK(b.intake.state() == IntakeState::running);
    b.run(0.05);
    CHECK(b.intake.state() == IntakeState::holding);
    CHECK(b.intake.unit_present());
    CHECK(b.out.volts == 0.0);
}

TEST_CASE("Intake: kurzes Bumper-Prellen startet kein Halten") {
    Bench b;
    b.intake.request_run(b.t);
    b.run(0.5);
    b.bumper = true;
    b.run(0.03);
    b.bumper = false;
    b.run(0.2);
    CHECK(b.intake.state() == IntakeState::running);
    CHECK_FALSE(b.intake.unit_present());
}

TEST_CASE("Intake: Einheit schon im Bumper -> sofort halten") {
    Bench b;
    b.bumper = true;
    b.intake.request_run(b.t);
    b.run(0.1);
    CHECK(b.intake.state() == IntakeState::holding);
}

TEST_CASE("Intake: Einheit geht verloren -> zurueck nach idle") {
    Bench b;
    b.intake.request_run(b.t);
    b.bumper = true;
    b.run(0.2);
    REQUIRE(b.intake.state() == IntakeState::holding);
    b.bumper = false;
    b.run(0.2);
    CHECK(b.intake.state() == IntakeState::idle);
    CHECK_FALSE(b.intake.unit_present());
}

TEST_CASE("Intake: Anlaufzeit zaehlt nicht als Stau") {
    Bench b;
    b.jammed = true;
    b.intake.request_run(b.t);
    b.run(0.28);  // grace_s = 0.3
    CHECK(b.intake.state() == IntakeState::running);
    CHECK(b.intake.jam_count() == 0);
}

TEST_CASE("Intake: Stau -> Rueckwaertsimpuls -> wieder vorwaerts") {
    Bench b;
    b.intake.request_run(b.t);
    b.run(0.4);
    b.jammed = true;
    b.run(0.45);  // Stillstand ab 0.4 s, jam_s = 0.25 -> Stau erkannt
    CHECK(b.intake.state() == IntakeState::reversing);
    CHECK(b.out.volts == doctest::Approx(-8.0));
    CHECK(b.intake.jam_count() == 1);
    b.jammed = false;  // der Impuls loest den Stau
    b.run(0.45);       // reverse_s = 0.4
    CHECK(b.intake.state() == IntakeState::running);
    CHECK(b.out.volts == doctest::Approx(10.0));
    b.run(1.0);
    CHECK(b.intake.state() == IntakeState::running);
    CHECK(b.intake.jam_count() == 1);
}

TEST_CASE("Intake: nach dem Rueckwaertsimpuls gilt die Anlaufzeit erneut") {
    Bench b;
    b.jammed = true;
    b.intake.request_run(b.t);
    b.run(0.3 + 0.25 + 0.05);
    REQUIRE(b.intake.state() == IntakeState::reversing);
    b.run(0.4);
    REQUIRE(b.intake.state() == IntakeState::running);
    b.run(0.28);
    CHECK(b.intake.state() == IntakeState::running);  // noch in der Anlaufzeit, kein zweiter Stau
    CHECK(b.intake.jam_count() == 1);
}

TEST_CASE("Intake: Stau bleibt -> nach max_retries Fehler, Rollen aus") {
    Bench b;
    b.jammed = true;  // der Impuls hilft nie
    b.intake.request_run(b.t);
    b.run(6.0);
    CHECK(b.intake.state() == IntakeState::error);
    CHECK(b.intake.error() == IntakeError::jam);
    CHECK(b.intake.jam_count() == 3);  // max_retries = 2 Impulse, der dritte Stau gibt auf
    CHECK(b.out.volts == 0.0);
}

TEST_CASE("Intake: nach Fehler neuer Befehl startet frisch") {
    Bench b;
    b.jammed = true;
    b.intake.request_run(b.t);
    b.run(6.0);
    REQUIRE(b.intake.state() == IntakeState::error);
    b.jammed = false;
    REQUIRE(b.intake.request_run(b.t));
    b.run(0.5);
    CHECK(b.intake.state() == IntakeState::running);
    CHECK(b.intake.error() == IntakeError::none);
    CHECK(b.intake.jam_count() == 0);
}

TEST_CASE("Intake: Timeout beim Lauf ohne Bumper") {
    Bench b;
    b.intake.request_run(b.t);
    b.run(3.9);
    CHECK(b.intake.state() == IntakeState::running);
    b.run(0.2);
    CHECK(b.intake.state() == IntakeState::idle);
    CHECK(b.intake.timed_out());
    CHECK(b.out.volts == 0.0);
}

TEST_CASE("Intake: Gesamtzeit des Laufs gilt auch mit Rueckwaertsimpulsen") {
    Bench b;
    b.cfg.run_timeout_s = 1.0;
    b.intake = IntakeSubsystem(b.cfg, 0.0);
    b.jammed = true;
    b.intake.request_run(b.t);
    b.run(1.2);
    CHECK(b.intake.state() == IntakeState::idle);
    CHECK(b.intake.timed_out());
}

TEST_CASE("Intake: Stopp") {
    Bench b;
    b.intake.request_run(b.t);
    b.run(0.5);
    b.intake.request_stop(b.t);
    b.tick();
    CHECK(b.intake.state() == IntakeState::idle);
    CHECK(b.out.volts == 0.0);
    CHECK_FALSE(b.intake.timed_out());
}

TEST_CASE("Intake: Sensor weg oder NaN -> Fehler und Rollen aus") {
    Bench b;
    b.intake.request_run(b.t);
    b.run(0.2);
    b.ok = false;
    b.tick();
    CHECK(b.intake.state() == IntakeState::error);
    CHECK(b.intake.error() == IntakeError::sensor);
    CHECK(b.out.volts == 0.0);

    Bench c;
    c.intake.request_run(c.t);
    c.run(0.2);
    c.vel = std::numeric_limits<double>::quiet_NaN();
    c.tick();
    CHECK(c.intake.error() == IntakeError::sensor);
}

TEST_CASE("Intake: ungueltige Konfiguration -> Fehler, kein Lauf") {
    IntakeConfig c = intake_config();
    c.run_timeout_s = 0.0;
    IntakeSubsystem i(c, 0.0);
    CHECK(i.error() == IntakeError::config);
    CHECK_FALSE(i.request_run(0.0));
    IntakeSensor s;
    CHECK(i.update(0.1, s).volts == 0.0);

    IntakeConfig d = intake_config();
    d.reverse_v = 3.0;  // Rueckwaerts muss negativ sein
    CHECK(IntakeSubsystem(d, 0.0).error() == IntakeError::config);
    IntakeConfig e = intake_config();
    e.run_v = -1.0;
    CHECK(IntakeSubsystem(e, 0.0).error() == IntakeError::config);
}

TEST_CASE("Intake: Spannung wird auf 12 V begrenzt") {
    IntakeConfig c = intake_config();
    c.run_v = 20.0;
    c.reverse_v = -20.0;
    IntakeSubsystem i(c, 0.0);
    i.request_run(0.0);
    IntakeSensor s;
    s.velocity_deg_s = 300.0;
    CHECK(i.update(0.01, s).volts == doctest::Approx(12.0));
}

TEST_CASE("Intake: config/ der beiden Roboter ist gueltig") {
    CHECK(IntakeSubsystem(config::stacker::intake, 0.0).error() == IntakeError::none);
    CHECK(IntakeSubsystem(config::runner::intake, 0.0).error() == IntakeError::none);
}
