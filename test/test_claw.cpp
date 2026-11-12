#include <cmath>
#include <limits>

#include "config/runner.hpp"
#include "config/stacker.hpp"
#include "doctest/doctest.h"
#include "subsystems/claw.hpp"

using namespace frostbyte;

namespace {

// Testwerte, unabhaengig von config/. Position: 0 = offen, waechst beim Schliessen.
ClawConfig claw_config() {
    ClawConfig c;
    c.open_v = -4.0;
    c.empty_travel_deg = 90.0;
    c.empty_margin_deg = 5.0;
    c.grace_s = 0.2;
    c.stall_deg_s = 10.0;
    c.plateau_s = 0.1;
    c.open_timeout_s = 1.5;
    c.close_timeout_s = 1.5;
    c.open_settle_s = 0.1;
    c.cup = {.close_v = 6.0, .hold_v = 2.0, .plateau_a = 1.2};
    c.pin = {.close_v = 4.0, .hold_v = 1.0, .plateau_a = 0.6};
    return c;
}

// Strecke: Geschwindigkeit proportional zur Spannung, harte Anschlaege bei 0 und full_deg, optional ein Objekt bei obj_deg.
// Stromaufnahme steigt mit der Spannung, sobald die Backe steht (Stall), sonst Leerlaufstrom.
struct Plant {
    double pos_deg = 0.0;
    double raw_offset_deg = 500.0;
    double gain = 60.0;  // Grad/s je Volt
    double full_deg = 100.0;
    double obj_deg = std::numeric_limits<double>::infinity();  // Objekt zwischen den Backen
    double amps_per_v = 0.3;
    double vel = 0.0;
    double amps = 0.0;
    bool connected = true;

    void step(double volts, double dt_s) {
        vel = gain * volts;
        const double next = pos_deg + vel * dt_s;
        const double stop_hi = std::min(full_deg, obj_deg);
        const double clamped = std::min(std::max(next, 0.0), stop_hi);
        const bool blocked = clamped != next;
        pos_deg = clamped;
        if (blocked)
            vel = 0.0;
        amps = blocked ? std::fabs(volts) * amps_per_v : 0.2;
    }
    ClawSensor sensor() const { return ClawSensor{pos_deg + raw_offset_deg, vel, amps, connected}; }
};

struct Bench {
    ClawConfig cfg = claw_config();
    ClawSubsystem claw{cfg, 0.0};
    Plant plant;
    double t = 0.0;
    ClawOutput out{};

    void tick() {
        t += 0.01;
        out = claw.update(t, plant.sensor());
        plant.step(out.volts, 0.01);
    }
    void run(double secs) {
        const int n = static_cast<int>(std::lround(secs / 0.01));
        for (int i = 0; i < n; ++i)
            tick();
    }
    void open_it() {
        REQUIRE(claw.request_open(t));
        run(1.0);
        REQUIRE(claw.state() == ClawState::open);
    }
};

}  // namespace

TEST_CASE("Claw: Lage unbekannt -> keine Spannung, Schliessen abgelehnt") {
    Bench b;
    b.run(0.5);
    CHECK(b.claw.state() == ClawState::unknown);
    CHECK(b.out.volts == 0.0);
    CHECK_FALSE(b.claw.request_close(ClawMode::cup, b.t));
}

TEST_CASE("Claw: Oeffnen faehrt bis zum Anschlag und nullt die Lage") {
    Bench b;
    b.plant.pos_deg = 40.0;
    REQUIRE(b.claw.request_open(b.t));
    b.run(0.1);
    CHECK(b.claw.state() == ClawState::opening);
    CHECK(b.out.volts == doctest::Approx(-4.0));
    b.run(1.0);
    CHECK(b.claw.state() == ClawState::open);
    CHECK(b.out.volts == 0.0);
    CHECK(b.plant.pos_deg == doctest::Approx(0.0));
}

TEST_CASE("Claw: Oeffnen laeuft in ein Timeout -> Fehler") {
    Bench b;
    b.plant.full_deg = 1e9;
    b.plant.pos_deg = 1e8;  // weit weg vom Anschlag: Oeffnen dauert laenger als das Timeout
    REQUIRE(b.claw.request_open(b.t));
    b.run(2.0);
    CHECK(b.claw.state() == ClawState::error);
    CHECK(b.claw.error() == ClawError::open_timeout);
    CHECK(b.out.volts == 0.0);
}

TEST_CASE("Claw: Cup greifen -> Plateau erkannt, Haltespannung") {
    Bench b;
    b.open_it();
    b.plant.obj_deg = 50.0;
    REQUIRE(b.claw.request_close(ClawMode::cup, b.t));
    b.run(0.15);
    CHECK(b.claw.state() == ClawState::closing);
    CHECK(b.out.volts == doctest::Approx(6.0));
    b.run(1.0);
    CHECK(b.claw.state() == ClawState::held);
    CHECK(b.claw.result() == ClawResult::gripped);
    CHECK(b.out.volts == doctest::Approx(2.0));
    CHECK(b.claw.travel_deg() == doctest::Approx(50.0));
}

TEST_CASE("Claw: Pin-Modus nutzt eigene Spannung und Schwelle") {
    Bench b;
    b.open_it();
    b.plant.obj_deg = 40.0;
    REQUIRE(b.claw.request_close(ClawMode::pin, b.t));
    b.run(0.1);
    CHECK(b.out.volts == doctest::Approx(4.0));
    b.run(1.2);
    // Strom im Stall: 4 V * 0.3 = 1.2 A >= 0.6 A (pin), waere im Cup-Modus (1.2 A) genau an der Schwelle
    CHECK(b.claw.state() == ClawState::held);
    CHECK(b.out.volts == doctest::Approx(1.0));
}

TEST_CASE("Claw: Stillstand mit zu kleinem Strom ist kein Griff -> Timeout, verfehlt, oeffnet wieder") {
    Bench b;
    b.open_it();
    b.plant.obj_deg = 50.0;
    b.plant.amps_per_v = 0.05;  // 6 V * 0.05 = 0.3 A < 1.2 A
    REQUIRE(b.claw.request_close(ClawMode::cup, b.t));
    b.run(1.6);
    CHECK(b.claw.result() == ClawResult::missed);
    CHECK(b.claw.state() == ClawState::opening);
    b.run(1.0);
    CHECK(b.claw.state() == ClawState::open);
}

TEST_CASE("Claw: Schliessen ins Leere -> verfehlt, oeffnet wieder") {
    Bench b;
    b.open_it();  // kein Objekt
    REQUIRE(b.claw.request_close(ClawMode::cup, b.t));
    b.run(1.45);  // 6 V * 60 = 360 Grad/s: in 0.3 s am Ende, Timeout 1.5 s
    CHECK(b.claw.result() == ClawResult::missed);
    CHECK(b.claw.state() != ClawState::held);
    b.run(1.5);
    CHECK(b.claw.state() == ClawState::open);
}

TEST_CASE("Claw: Anlaufstrom in der Anlaufzeit zaehlt nicht") {
    Bench b;
    b.open_it();
    b.plant.obj_deg = 0.5;  // Objekt praktisch schon beruehrt: Stall und Strom ab dem ersten Takt
    REQUIRE(b.claw.request_close(ClawMode::cup, b.t));
    b.run(0.15);  // grace_s = 0.2
    CHECK(b.claw.state() == ClawState::closing);
    b.run(0.3);
    CHECK(b.claw.state() == ClawState::held);
}

TEST_CASE("Claw: Plateau muss plateau_s halten") {
    Bench b;
    b.open_it();
    b.plant.obj_deg = 50.0;
    REQUIRE(b.claw.request_close(ClawMode::cup, b.t));
    // Fahrt bis zum Objekt: 50 Grad bei 360 Grad/s = 0.14 s, danach grace 0.2 s, plateau 0.1 s
    b.run(0.25);
    CHECK(b.claw.state() == ClawState::closing);
    b.run(0.2);
    CHECK(b.claw.state() == ClawState::held);
}

TEST_CASE("Claw: Oeffnen aus held") {
    Bench b;
    b.open_it();
    b.plant.obj_deg = 50.0;
    b.claw.request_close(ClawMode::cup, b.t);
    b.run(1.5);
    REQUIRE(b.claw.state() == ClawState::held);
    b.plant.obj_deg = std::numeric_limits<double>::infinity();
    REQUIRE(b.claw.request_open(b.t));
    b.run(1.5);
    CHECK(b.claw.state() == ClawState::open);
    CHECK(b.claw.result() == ClawResult::gripped);  // letztes Ergebnis bleibt bis zum naechsten Schliessen
}

TEST_CASE("Claw: Sensorausfall -> Fehler, 0 V, Lage unbekannt; Oeffnen heilt") {
    Bench b;
    b.open_it();
    b.plant.connected = false;
    b.tick();
    CHECK(b.claw.state() == ClawState::error);
    CHECK(b.claw.error() == ClawError::sensor);
    CHECK(b.out.volts == 0.0);
    CHECK_FALSE(b.claw.request_close(ClawMode::cup, b.t));
    b.plant.connected = true;
    REQUIRE(b.claw.request_open(b.t));
    b.run(1.0);
    CHECK(b.claw.state() == ClawState::open);
    CHECK(b.claw.error() == ClawError::none);
}

TEST_CASE("Claw: NaN im Sensor -> Fehler") {
    Bench b;
    b.open_it();
    ClawSensor s = b.plant.sensor();
    s.current_a = std::numeric_limits<double>::quiet_NaN();
    b.t += 0.01;
    CHECK(b.claw.update(b.t, s).volts == 0.0);
    CHECK(b.claw.error() == ClawError::sensor);
}

TEST_CASE("Claw: ungueltige Konfiguration -> Fehler, nichts faehrt") {
    ClawConfig c = claw_config();
    c.cup.close_v = -1.0;  // Schliessen muss positiv sein
    ClawSubsystem a(c, 0.0);
    CHECK(a.error() == ClawError::config);
    CHECK_FALSE(a.request_open(0.0));

    ClawConfig d = claw_config();
    d.open_v = 2.0;
    CHECK(ClawSubsystem(d, 0.0).error() == ClawError::config);
    ClawConfig e = claw_config();
    e.pin.plateau_a = 0.0;
    CHECK(ClawSubsystem(e, 0.0).error() == ClawError::config);
    ClawConfig f = claw_config();
    f.empty_travel_deg = 3.0;  // kleiner als der Rand
    CHECK(ClawSubsystem(f, 0.0).error() == ClawError::config);
}

TEST_CASE("Claw: Spannung wird auf 12 V begrenzt") {
    ClawConfig c = claw_config();
    c.cup.close_v = 30.0;
    c.open_v = -30.0;
    Bench b;
    b.cfg = c;
    b.claw = ClawSubsystem(c, 0.0);
    b.plant.pos_deg = 40.0;
    b.claw.request_open(b.t);
    b.tick();
    CHECK(b.out.volts == doctest::Approx(-12.0));
}

TEST_CASE("Claw: config/ ist gesperrt, bis Weg und Stromschwellen gemessen sind (M-13)") {
    CHECK(ClawSubsystem(config::stacker::claw, 0.0).error() == ClawError::config);
    CHECK(ClawSubsystem(config::runner::claw, 0.0).error() == ClawError::config);
    CHECK_FALSE(ClawSubsystem(ClawConfig{}, 0.0).request_open(0.0));  // Standardwerte raten nichts
}

TEST_CASE("Claw: Modi haben getrennte Schwellen") {
    // Strom im Stall: Pin 4 V * 0.18 = 0.72 A >= 0.6 A -> Griff; Cup 6 V * 0.18 = 1.08 A < 1.2 A -> kein Griff.
    Bench pin;
    pin.open_it();
    pin.plant.obj_deg = 40.0;
    pin.plant.amps_per_v = 0.18;
    pin.claw.request_close(ClawMode::pin, pin.t);
    pin.run(1.0);
    CHECK(pin.claw.state() == ClawState::held);

    Bench cup;
    cup.open_it();
    cup.plant.obj_deg = 40.0;
    cup.plant.amps_per_v = 0.18;
    cup.claw.request_close(ClawMode::cup, cup.t);
    cup.run(1.0);
    CHECK(cup.claw.state() == ClawState::closing);
    cup.run(0.6);
    CHECK(cup.claw.result() == ClawResult::missed);
}

// Grenzfaelle: direkt gefuetterte Sensorwerte statt der Strecke. Alle Zeiten sind Vielfache von 0.125 s (exakt in double),
// damit "genau auf der Grenze" wirklich genau ist. Der Takt ist dt = 0.125 s; das erste update() hat dt = 0.
namespace {

constexpr double dt = 0.125;

ClawConfig edge_config() {
    ClawConfig c;
    c.open_v = -4.0;
    c.empty_travel_deg = 90.0;
    c.empty_margin_deg = 5.0;  // leer ab 85 Grad Weg
    c.grace_s = 0.25;
    c.stall_deg_s = 10.0;
    c.plateau_s = 0.125;
    c.open_timeout_s = 100.0;
    c.close_timeout_s = 100.0;
    c.open_settle_s = 0.125;
    c.cup = {.close_v = 6.0, .hold_v = 2.0, .plateau_a = 1.5};
    c.pin = {.close_v = 4.0, .hold_v = 1.0, .plateau_a = 0.5};
    return c;
}

constexpr double open_pos = 100.0;

ClawSensor still(double pos_deg, double amps = 0.0) { return ClawSensor{pos_deg, 0.0, amps, true}; }

// Treibt ein Subsystem im Takt dt mit gleichbleibendem Sensor; t ist die Zeit des letzten update().
struct Stepper {
    ClawSubsystem& claw;
    double t = 0.0;
    void step(const ClawSensor& s) {
        t += dt;
        claw.update(t, s);
    }
};

// Oeffnen bis "open" mit der Lage open_pos.
void open_claw(Stepper& k) {
    REQUIRE(k.claw.request_open(k.t));
    k.claw.update(k.t, still(open_pos));  // dt = 0
    k.step(still(open_pos));              // elapsed 0.125 < grace_s
    k.step(still(open_pos));              // elapsed 0.25 == grace_s, Stillstand 0.125 == open_settle_s
    REQUIRE(k.claw.state() == ClawState::open);
}

}  // namespace

TEST_CASE("Claw: Oeffnen zaehlt ab grace_s einschliesslich (elapsed == grace_s ist die Anlaufzeit vorbei)") {
    ClawConfig c = edge_config();  // open_settle_s = dt: ein Takt Stillstand reicht
    ClawSubsystem claw{c, 0.0};
    Stepper k{claw};
    REQUIRE(claw.request_open(0.0));
    claw.update(0.0, still(open_pos));
    k.step(still(open_pos));
    CHECK(claw.state() == ClawState::opening);  // elapsed 0.125 < grace_s
    k.step(still(open_pos));                    // elapsed == grace_s
    CHECK(claw.state() == ClawState::open);
}

TEST_CASE("Claw: Oeffnen ist fertig, sobald der Stillstand open_settle_s erreicht (einschliesslich)") {
    ClawConfig c = edge_config();
    c.open_settle_s = 0.25;  // zwei Takte Stillstand
    c.grace_s = 0.125;
    ClawSubsystem claw{c, 0.0};
    Stepper k{claw};
    REQUIRE(claw.request_open(0.0));
    claw.update(0.0, still(open_pos));
    k.step(still(open_pos));  // grace erreicht, Stillstand 0.125
    CHECK(claw.state() == ClawState::opening);
    k.step(still(open_pos));  // Stillstand == open_settle_s
    CHECK(claw.state() == ClawState::open);
}

TEST_CASE("Claw: leer gegriffen genau bei empty_travel_deg - empty_margin_deg, knapp davor nicht") {
    ClawConfig c = edge_config();
    ClawSubsystem claw{c, 0.0};
    Stepper k{claw};
    open_claw(k);
    REQUIRE(claw.request_close(ClawMode::cup, k.t));
    k.step(still(open_pos + 84.875));  // 0.125 Grad vor der Grenze, Strom 0: kein Plateau
    CHECK(claw.state() == ClawState::closing);
    CHECK(claw.result() == ClawResult::none);
    k.step(still(open_pos + 85.0));  // genau auf der Grenze
    CHECK(claw.result() == ClawResult::missed);
    CHECK(claw.state() == ClawState::opening);
}

TEST_CASE("Claw: Plateau-Strom genau plateau_a zaehlt, knapp darunter nicht") {
    for (const double amps : {1.5, 1.4999}) {
        INFO("Strom " << amps);
        ClawConfig c = edge_config();  // plateau_s == dt: ein Takt reicht
        ClawSubsystem claw{c, 0.0};
        Stepper k{claw};
        open_claw(k);
        REQUIRE(claw.request_close(ClawMode::cup, k.t));
        k.step(still(open_pos + 40.0, amps));  // elapsed 0.125 < grace_s: zaehlt nicht
        CHECK(claw.state() == ClawState::closing);
        k.step(still(open_pos + 40.0, amps));  // elapsed == grace_s
        if (amps >= 1.5) {
            CHECK(claw.state() == ClawState::held);
            CHECK(claw.result() == ClawResult::gripped);
        } else {
            CHECK(claw.state() == ClawState::closing);
            CHECK(claw.result() == ClawResult::none);
        }
    }
}

TEST_CASE("Claw: Plateau gilt, sobald es plateau_s erreicht (einschliesslich), nicht einen Takt spaeter") {
    ClawConfig c = edge_config();
    c.plateau_s = 0.25;  // zwei Takte
    ClawSubsystem claw{c, 0.0};
    Stepper k{claw};
    open_claw(k);
    REQUIRE(claw.request_close(ClawMode::cup, k.t));
    k.step(still(open_pos + 40.0, 1.5));  // elapsed 0.125 < grace_s
    k.step(still(open_pos + 40.0, 1.5));  // erster Takt mit Plateau: 0.125
    CHECK(claw.state() == ClawState::closing);
    k.step(still(open_pos + 40.0, 1.5));  // == plateau_s
    CHECK(claw.state() == ClawState::held);
}
