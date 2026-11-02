#include <algorithm>
#include <cmath>
#include <limits>

#include "doctest/doctest.h"
#include "subsystems/lift.hpp"

using namespace frostbyte;

namespace {

// Testgeometrie (nicht aus config/): Schwenkarm, Hoehe = 10 + 20 sin(Winkel), Winkel 0..90 Grad.
double arm_height(double deg) { return 10.0 + 20.0 * std::sin(deg * 3.14159265358979323846 / 180.0); }

LiftConfig lift_config() {
    LiftConfig c;
    c.cal_angle_deg = {0, 22.5, 45, 67.5, 90};
    for (int i = 0; i < 5; ++i)
        c.cal_height_in[i] = arm_height(c.cal_angle_deg[i]);
    c.min_in = 10.0;
    c.max_in = 30.0;
    c.kp_v_per_deg = 0.5;
    c.kg_v = 1.5;
    return c;
}

// Strecke: Geschwindigkeit proportional zur Spannung abzueglich der Schwerkraft-Haltespannung, harte Anschlaege.
// Der Sensor liest den Winkel mit einem beliebigen Rohversatz (der Lift kennt ihn erst nach dem Homing).
struct Plant {
    double pos_deg = 40.0;  // wahre Lage relativ zum unteren Anschlag
    double gain = 20.0;     // Grad/s je Volt
    double hold_v = 1.5;
    double low_deg = 0.0, high_deg = 95.0;
    double raw_offset_deg = 1234.5;
    bool stuck = false;  // blockiert: bewegt sich nicht, egal welche Spannung
    bool connected = true;
    bool has_low_stop = true;
    double vel = 0.0;

    void step(double volts, bool brake, double dt_s) {
        if (stuck || brake) {
            vel = 0.0;
            return;
        }
        vel = gain * (volts - hold_v);
        pos_deg += vel * dt_s;
        if (has_low_stop && pos_deg <= low_deg) {
            pos_deg = low_deg;
            vel = std::max(vel, 0.0);
        }
        if (pos_deg >= high_deg) {
            pos_deg = high_deg;
            vel = std::min(vel, 0.0);
        }
    }
    LiftSensor sensor() const { return LiftSensor{pos_deg + raw_offset_deg, vel, connected}; }
    double height() const { return arm_height(std::clamp(pos_deg, 0.0, 90.0)); }
};

struct Bench {
    explicit Bench(const LiftConfig& c = lift_config()) : lift(c, 0.0) {}

    void run(double seconds) {
        const int n = static_cast<int>(seconds / 0.01 + 0.5);
        for (int i = 0; i < n; ++i) {
            now += 0.01;
            out = lift.update(now, plant.sensor());
            peak_v = std::max(peak_v, std::fabs(out.volts));
            plant.step(out.volts, out.brake, 0.01);
            max_height = std::max(max_height, plant.height());
        }
    }
    // Laeuft, bis der Zustand erreicht ist (oder max_s vorbei ist); gibt die vergangene Zeit zurueck.
    double run_until(LiftState s, double max_s) {
        const double t0 = now;
        while (now - t0 < max_s && lift.state() != s)
            run(0.01);
        return now - t0;
    }
    void home() {
        REQUIRE(lift.request_home(now));
        run_until(LiftState::holding, 5.0);
    }

    LiftSubsystem lift;
    Plant plant;
    LiftOutput out;
    double now = 0.0;
    double peak_v = 0.0, max_height = 0.0;
};

}  // namespace

TEST_CASE("Lift: ungueltige Konfiguration -> Fehler, kein Homing, 0 V mit Bremse") {
    LiftConfig c = lift_config();
    c.cal_height_in[2] = c.cal_height_in[1];  // nicht streng steigend
    LiftSubsystem lift(c, 0.0);
    CHECK(lift.state() == LiftState::error);
    CHECK(lift.error() == LiftError::config);
    CHECK_FALSE(lift.request_home(0.0));
    const LiftOutput o = lift.update(0.01, LiftSensor{});
    CHECK(o.volts == 0.0);
    CHECK(o.brake);
    LiftConfig d = lift_config();
    d.min_in = 31.0;  // min > max
    CHECK(LiftSubsystem(d, 0.0).error() == LiftError::config);
}

TEST_CASE("Lift: vor dem Homing 0 V mit Bremse, Fahrbefehle werden abgelehnt") {
    Bench b;
    CHECK(b.lift.state() == LiftState::unhomed);
    b.run(0.1);
    CHECK(b.out.volts == 0.0);
    CHECK(b.out.brake);
    CHECK_FALSE(b.lift.request_move(20.0, b.now));
    CHECK_FALSE(b.lift.request_hold(b.now));
    CHECK(b.lift.state() == LiftState::unhomed);
}

TEST_CASE("Lift: Homing aus der Mitte findet den Anschlag, Rohversatz faellt heraus") {
    Bench b;
    b.plant.pos_deg = 60.0;
    b.home();
    CHECK(b.lift.state() == LiftState::holding);
    CHECK(b.lift.homed());
    CHECK(b.plant.pos_deg == doctest::Approx(0.0).epsilon(0.01).scale(1.0));
    CHECK(b.lift.height_in() == doctest::Approx(10.0));
    b.run(1.0);
    CHECK(b.lift.state() == LiftState::holding);
    CHECK(b.plant.pos_deg < 1.0);  // bleibt unten
}

TEST_CASE("Lift: Homing, wenn der Lift schon unten steht") {
    Bench b;
    b.plant.pos_deg = 0.0;
    b.home();
    CHECK(b.lift.state() == LiftState::holding);
    CHECK(b.lift.homed());
}

TEST_CASE("Lift: Homing ohne Anschlag -> Timeout, Fehler, Bremse") {
    Bench b;
    b.plant.has_low_stop = false;
    b.plant.low_deg = -1e9;
    REQUIRE(b.lift.request_home(b.now));
    b.run(lift_config().home_timeout_s + 0.5);
    CHECK(b.lift.state() == LiftState::error);
    CHECK(b.lift.error() == LiftError::home_timeout);
    CHECK_FALSE(b.lift.homed());
    CHECK(b.out.volts == 0.0);
    CHECK(b.out.brake);
    CHECK_FALSE(b.lift.request_move(20.0, b.now));
}

TEST_CASE("Lift: Hoehe anfahren, ankommen innerhalb Toleranz und Timeout, Spannungsgrenzen eingehalten") {
    Bench b;
    b.home();
    b.peak_v = 0.0;
    REQUIRE(b.lift.request_move(22.0, b.now));
    CHECK(b.lift.state() == LiftState::moving);
    const double t = b.run_until(LiftState::holding, lift_config().move_timeout_s + 0.5);
    CHECK(b.lift.state() == LiftState::holding);
    CHECK_FALSE(b.lift.timed_out());
    CHECK(t < lift_config().move_timeout_s);
    CHECK(std::fabs(b.plant.height() - 22.0) <= 0.3);
    b.run(1.0);
    CHECK(std::fabs(b.plant.height() - 22.0) <= 0.3);  // haelt
    CHECK(b.peak_v <= lift_config().max_up_v + 1e-9);
    REQUIRE(b.lift.request_move(12.0, b.now));  // wieder runter
    b.run_until(LiftState::holding, 3.5);
    CHECK(std::fabs(b.plant.height() - 12.0) <= 0.3);
}

TEST_CASE("Lift: Ziele ausserhalb der Endlagen werden begrenzt") {
    Bench b;
    b.home();
    REQUIRE(b.lift.request_move(99.0, b.now));
    b.run(3.5);
    CHECK(b.max_height <= 30.0 + 0.3);
    CHECK(std::fabs(b.plant.height() - 30.0) <= 0.3);
    REQUIRE(b.lift.request_move(-5.0, b.now));
    b.run(3.5);
    CHECK(std::fabs(b.plant.height() - 10.0) <= 0.3);
    CHECK_FALSE(b.lift.request_move(std::numeric_limits<double>::quiet_NaN(), b.now));
}

TEST_CASE("Lift: Software-Endlage sperrt die Spannung nach aussen") {
    LiftConfig c = lift_config();
    c.max_in = 25.0;  // Endlage unter dem Tabellenende
    c.kg_v = 8.0;     // starke Haltespannung: der Regler wuerde auch ueber der Endlage noch nach oben druecken
    c.kp_v_per_deg = 0.05;
    Bench b(c);
    b.plant.hold_v = 8.0;
    b.home();
    REQUIRE(b.lift.request_move(25.0, b.now));
    LiftSensor over = b.plant.sensor();
    over.position_deg = 70.0 + b.plant.raw_offset_deg;  // Hoehe ~28,8 > 25 + Spiel
    over.velocity_deg_s = 0.0;
    const LiftOutput o = b.lift.update(b.now + 0.01, over);
    CHECK(b.lift.height_in() > c.max_in + c.margin_in);
    CHECK(o.volts <= 0.0);                              // ueber max+Spiel: nie nach oben
    over.position_deg = b.plant.raw_offset_deg + 20.0;  // normal darunter: Regler darf nach oben
    CHECK(b.lift.update(b.now + 0.02, over).volts > 0.0);
}

TEST_CASE("Lift: Decke wird gesenkt -> Lift faehrt von selbst nach unten und bleibt dort") {
    Bench b;
    b.home();
    REQUIRE(b.lift.request_move(30.0, b.now));
    b.run(3.5);
    REQUIRE(std::fabs(b.plant.height() - 30.0) <= 0.3);
    b.lift.set_ceiling_in(22.0);
    b.run(0.2);
    CHECK(b.lift.state() == LiftState::moving);  // sichtbar als Fahrt, mit Timeout
    b.run(3.3);
    CHECK(std::fabs(b.plant.height() - 22.0) <= 0.3);
    CHECK(b.lift.state() == LiftState::holding);
    // Neuer Befehl ueber der Decke wird auf die Decke begrenzt.
    REQUIRE(b.lift.request_move(28.0, b.now));
    b.run(3.5);
    CHECK(std::fabs(b.plant.height() - 22.0) <= 0.3);
    // Decke wieder hoch: der Lift bleibt, wo er ist, bis ein neuer Befehl kommt.
    b.lift.set_ceiling_in(30.0);
    b.run(1.0);
    CHECK(std::fabs(b.plant.height() - 22.0) <= 0.3);
}

TEST_CASE("Lift: blockiert -> Fahr-Timeout, Zustand holding, timed_out, Spannung bleibt (kein freies Fallen)") {
    Bench b;
    b.home();
    b.plant.stuck = true;
    REQUIRE(b.lift.request_move(25.0, b.now));
    b.run(lift_config().move_timeout_s + 0.3);
    CHECK(b.lift.state() == LiftState::holding);
    CHECK(b.lift.timed_out());
    CHECK_FALSE(b.out.brake);
    // Ziel ist jetzt die gemessene Hoehe: Haltespannung statt weiter gegen die Blockade zu druecken.
    CHECK(b.out.volts == doctest::Approx(lift_config().kg_v).epsilon(0.05));
    CHECK(b.lift.target_in() == doctest::Approx(10.0).epsilon(0.05));
    // Ein neuer Befehl setzt timed_out zurueck.
    b.plant.stuck = false;
    REQUIRE(b.lift.request_move(15.0, b.now));
    CHECK_FALSE(b.lift.timed_out());
}

TEST_CASE("Lift: Sensor weg oder NaN -> Fehler, 0 V mit Bremse, nur nach neuem Homing wieder fahrbar") {
    for (bool use_nan : {false, true}) {
        Bench b;
        b.home();
        REQUIRE(b.lift.request_move(20.0, b.now));
        b.run(0.1);
        if (use_nan)
            b.plant.raw_offset_deg = std::numeric_limits<double>::quiet_NaN();
        else
            b.plant.connected = false;
        b.run(0.05);
        CHECK(b.lift.state() == LiftState::error);
        CHECK(b.lift.error() == LiftError::sensor);
        CHECK(b.out.volts == 0.0);
        CHECK(b.out.brake);
        CHECK_FALSE(b.lift.homed());
        CHECK_FALSE(b.lift.request_move(20.0, b.now));
        // Sensor wieder da: erst Homing, dann Fahren.
        b.plant.raw_offset_deg = 77.0;
        b.plant.connected = true;
        b.plant.stuck = false;
        b.home();
        CHECK(b.lift.homed());
        CHECK(b.lift.error() == LiftError::none);
        CHECK(b.lift.request_move(20.0, b.now));
    }
}

TEST_CASE("Lift: Hoehe und Winkel hin und zurueck, begrenzt an den Tabellenenden") {
    LiftSubsystem lift(lift_config(), 0.0);
    for (double h = 10.0; h <= 30.0; h += 0.7)
        CHECK(lift.height_for_angle(lift.angle_for_height(h)) == doctest::Approx(h).epsilon(1e-6).scale(1.0));
    CHECK(lift.angle_for_height(-5.0) == 0.0);
    CHECK(lift.angle_for_height(99.0) == 90.0);
}
