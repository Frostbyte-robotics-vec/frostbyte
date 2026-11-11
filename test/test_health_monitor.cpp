#include <cmath>
#include <cstring>
#include <limits>
#include <string>

#include "doctest/doctest.h"
#include "hal/interface/motor.hpp"
#include "util/health_monitor.hpp"

using namespace frostbyte;

namespace {

class FakeMotor : public hal::Motor {
public:
    void set_voltage_v(double) override {}
    void stop() override {}
    double position_deg() const override { return 0.0; }
    double velocity_deg_s() const override { return 0.0; }
    double temperature_c() const override { return temp_c; }
    double current_a() const override { return 0.0; }
    bool connected() const override { return is_connected; }

    bool is_connected = true;
    double temp_c = 30.0;
};

constexpr double nan_v = std::numeric_limits<double>::quiet_NaN();

HealthInput ok_input() {
    HealthInput in;
    in.battery_v = 12.5;
    return in;
}

}  // namespace

TEST_CASE("Health: alles in Ordnung -> keine Warnung") {
    FakeMotor lift, intake;
    HealthMonitor<4> h{HealthConfig{}};
    REQUIRE(h.add_motor(lift, "Lift"));
    REQUIRE(h.add_motor(intake, "Intake"));
    const HealthStatus s = h.update(ok_input());
    CHECK(s.warning == HealthWarning::none);
    CHECK(s.motor_index == -1);
    CHECK_FALSE(s.any());
}

TEST_CASE("Health: Kabel gezogen -> erst die zweite Messung in Folge warnt, ein Aussetzer nicht") {
    FakeMotor lift;
    HealthMonitor<4> h{HealthConfig{}};
    REQUIRE(h.add_motor(lift, "Lift"));
    lift.is_connected = false;
    CHECK(h.update(ok_input()).warning == HealthWarning::none);  // 1. Messung: noch Entprellung
    lift.is_connected = true;                                    // Aussetzer vorbei
    CHECK(h.update(ok_input()).warning == HealthWarning::none);
    lift.is_connected = false;
    CHECK(h.update(ok_input()).warning == HealthWarning::none);
    const HealthStatus s = h.update(ok_input());
    CHECK(s.warning == HealthWarning::motor_lost);
    CHECK(s.motor_index == 0);
    lift.is_connected = true;  // Kabel wieder dran: Warnung geht sofort
    CHECK(h.update(ok_input()).warning == HealthWarning::none);
}

TEST_CASE(
    "Health: Kabel ziehen -> Warnung in höchstens 200 ms bei 100-ms-Takt (jeder Zeitpunkt zwischen zwei Messungen)") {
    constexpr double period_s = 0.1;
    for (int pull_ms = 0; pull_ms < 100; ++pull_ms) {  // Kabel wird pull_ms nach einer Messung gezogen
        FakeMotor lift;
        HealthMonitor<4> h{HealthConfig{}};
        REQUIRE(h.add_motor(lift, "Lift"));
        for (int i = 0; i < 5; ++i)
            REQUIRE(h.update(ok_input()).warning == HealthWarning::none);
        const double t_pull = 5 * period_s - period_s + pull_ms / 1000.0;  // nach der 5. Messung bei 0.4 s
        lift.is_connected = false;
        double t_warn = -1.0;
        for (int i = 5; i < 12 && t_warn < 0; ++i)
            if (h.update(ok_input()).warning == HealthWarning::motor_lost)
                t_warn = i * period_s;
        REQUIRE(t_warn > 0);
        CHECK(t_warn - t_pull <= 0.2 + 1e-9);
    }
}

TEST_CASE("Health: Temperatur mit Hysterese; getrennter Motor zählt nicht als kalt oder heiß") {
    FakeMotor lift;
    HealthConfig cfg;
    cfg.temp_warn_c = 55.0;
    cfg.temp_clear_c = 50.0;
    HealthMonitor<4> h{cfg};
    REQUIRE(h.add_motor(lift, "Lift"));
    lift.temp_c = 54.9;
    CHECK(h.update(ok_input()).warning == HealthWarning::none);
    lift.temp_c = 55.0;
    CHECK(h.update(ok_input()).warning == HealthWarning::motor_hot);
    lift.temp_c = 52.0;  // über der Rückstellschwelle: bleibt
    CHECK(h.update(ok_input()).warning == HealthWarning::motor_hot);
    lift.temp_c = 49.9;
    CHECK(h.update(ok_input()).warning == HealthWarning::none);
    lift.temp_c = 80.0;
    lift.is_connected = false;  // Wert eines getrennten Motors ist bedeutungslos
    (void)h.update(ok_input());
    CHECK(h.update(ok_input()).warning == HealthWarning::motor_lost);
}

TEST_CASE("Health: Akku mit Hysterese, unbekannte Spannung (NaN) warnt nicht") {
    HealthConfig cfg;
    cfg.battery_warn_v = 11.0;
    cfg.battery_clear_v = 11.5;
    HealthMonitor<2> h{cfg};
    HealthInput in;
    in.battery_v = nan_v;
    CHECK(h.update(in).warning == HealthWarning::none);
    in.battery_v = 11.1;
    CHECK(h.update(in).warning == HealthWarning::none);
    in.battery_v = 10.9;
    CHECK(h.update(in).warning == HealthWarning::battery_low);
    in.battery_v = 11.3;  // unter der Rückstellschwelle: bleibt
    CHECK(h.update(in).warning == HealthWarning::battery_low);
    in.battery_v = 11.6;
    CHECK(h.update(in).warning == HealthWarning::none);
}

TEST_CASE("Health: Takt-Warnung vom Steuer-Task wird durchgereicht") {
    HealthMonitor<2> h{HealthConfig{}};
    HealthInput in = ok_input();
    in.cycle_warning = true;
    CHECK(h.update(in).warning == HealthWarning::cycle_overrun);
    in.cycle_warning = false;
    CHECK(h.update(in).warning == HealthWarning::none);
}

TEST_CASE("Health: bei mehreren Warnungen gilt die wichtigste; erster betroffener Motor") {
    FakeMotor a, b;
    HealthMonitor<4> h{HealthConfig{}};
    REQUIRE(h.add_motor(a, "A"));
    REQUIRE(h.add_motor(b, "B"));
    HealthInput in;
    in.battery_v = 9.0;
    in.cycle_warning = true;
    CHECK(h.update(in).warning == HealthWarning::battery_low);  // Akku vor Takt
    b.temp_c = 70.0;
    HealthStatus s = h.update(in);
    CHECK(s.warning == HealthWarning::motor_hot);  // heiß vor Akku
    CHECK(s.motor_index == 1);
    a.is_connected = false;
    b.is_connected = false;
    (void)h.update(in);
    s = h.update(in);
    CHECK(s.warning == HealthWarning::motor_lost);  // getrennt vor heiß
    CHECK(s.motor_index == 0);
}

TEST_CASE("Health: mehr Motoren als Platz -> add_motor lehnt ab, nichts wird überschrieben") {
    FakeMotor a, b;
    HealthMonitor<1> h{HealthConfig{}};
    CHECK(h.add_motor(a, "A"));
    CHECK_FALSE(h.add_motor(b, "B"));
    b.is_connected = false;
    (void)h.update(ok_input());
    CHECK(h.update(ok_input()).warning == HealthWarning::none);  // b wird nicht überwacht
}

TEST_CASE("Health: Text für den Controller und Ereignis-Token für das Log") {
    FakeMotor lift;
    HealthMonitor<2> h{HealthConfig{}};
    REQUIRE(h.add_motor(lift, "Lift"));
    char buf[32];

    HealthStatus none;
    CHECK(std::string(h.text(none, buf, sizeof buf)) == "");
    CHECK(std::string(h.event_token(none, buf, sizeof buf)) == "");

    lift.is_connected = false;
    (void)h.update(ok_input());
    HealthStatus s = h.update(ok_input());
    CHECK(std::string(h.text(s, buf, sizeof buf)) == "AUS Lift");
    CHECK(std::string(h.event_token(s, buf, sizeof buf)) == "HEALTH:MOTOR_LOST:Lift");

    lift.is_connected = true;
    lift.temp_c = 60.0;
    s = h.update(ok_input());
    CHECK(std::string(h.text(s, buf, sizeof buf)) == "HEISS Lift");
    CHECK(std::string(h.event_token(s, buf, sizeof buf)) == "HEALTH:MOTOR_HOT:Lift");

    lift.temp_c = 30.0;
    HealthInput in = ok_input();
    in.battery_v = 9.0;
    s = h.update(in);
    CHECK(std::string(h.text(s, buf, sizeof buf)) == "AKKU NIEDRIG");
    CHECK(std::string(h.event_token(s, buf, sizeof buf)) == "HEALTH:BATTERY_LOW");

    in.battery_v = 12.5;
    in.cycle_warning = true;
    s = h.update(in);
    CHECK(std::string(h.text(s, buf, sizeof buf)) == "TAKT");
    CHECK(std::string(h.event_token(s, buf, sizeof buf)) == "HEALTH:CYCLE_OVERRUN");

    char tiny[5];
    CHECK(std::strlen(h.text(s, tiny, sizeof tiny)) == 4);  // abgeschnitten, nie Überlauf
}

TEST_CASE("SharedHealth: Zustand geht heil von einem Task zum anderen") {
    SharedHealth shared;
    CHECK_FALSE(shared.load().any());
    HealthStatus s;
    s.warning = HealthWarning::motor_hot;
    s.motor_index = 2;
    shared.publish(s);
    const HealthStatus back = shared.load();
    CHECK(back.warning == HealthWarning::motor_hot);
    CHECK(back.motor_index == 2);
}
