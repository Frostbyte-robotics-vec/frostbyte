#include "doctest/doctest.h"
#include "hal/sim/sim_sensors.hpp"

using namespace frostbyte::hal;

TEST_CASE("SimRotation: Werte setzen, zurücksetzen, Ausfall") {
    SimRotation r;
    r.set_position_deg(90.0);
    r.set_velocity_deg_s(10.0);
    CHECK(r.position_deg() == doctest::Approx(90.0));
    CHECK(r.velocity_deg_s() == doctest::Approx(10.0));
    r.reset();
    CHECK(r.position_deg() == doctest::Approx(0.0));
    CHECK(r.connected());
    r.set_connected(false);
    CHECK_FALSE(r.connected());
}

TEST_CASE("SimImu: Heading liegt immer in (-180, 180]") {
    SimImu imu;
    imu.set_heading_deg(190.0);
    CHECK(imu.heading_deg() == doctest::Approx(-170.0));
    CHECK_FALSE(imu.calibrating());
    imu.set_calibrating(true);
    CHECK(imu.calibrating());
}

TEST_CASE("SimGps: ohne Messung ungültig") {
    SimGps gps;
    CHECK_FALSE(gps.read().valid);
    GpsReading g;
    g.valid = true;
    g.x_in = 12.0;
    g.y_in = -6.0;
    g.heading_deg = 45.0;
    g.error_in = 0.5;
    gps.set_reading(g);
    CHECK(gps.read().valid);
    CHECK(gps.read().x_in == doctest::Approx(12.0));
    CHECK(gps.read().error_in == doctest::Approx(0.5));
}
