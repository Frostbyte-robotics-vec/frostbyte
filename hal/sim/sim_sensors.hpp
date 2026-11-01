#pragma once

#include "core/units.hpp"
#include "hal/interface/gps.hpp"
#include "hal/interface/imu.hpp"
#include "hal/interface/rotation_sensor.hpp"

// Fake-Sensoren: Die Simulation bzw. der Test setzt die Werte.
namespace frostbyte::hal {

class SimRotation : public RotationSensor {
public:
    double position_deg() const override { return position_deg_; }
    double velocity_deg_s() const override { return velocity_deg_s_; }
    void reset() override { position_deg_ = 0.0; }
    bool connected() const override { return connected_; }

    void set_position_deg(double v) { position_deg_ = v; }
    void set_velocity_deg_s(double v) { velocity_deg_s_ = v; }
    void set_connected(bool c) { connected_ = c; }

private:
    double position_deg_ = 0.0;
    double velocity_deg_s_ = 0.0;
    bool connected_ = true;
};

class SimImu : public Imu {
public:
    double heading_deg() const override { return heading_deg_; }
    bool calibrating() const override { return calibrating_; }
    bool connected() const override { return connected_; }

    void set_heading_deg(double v) { heading_deg_ = wrap_deg(v); }
    void set_calibrating(bool c) { calibrating_ = c; }
    void set_connected(bool c) { connected_ = c; }

private:
    double heading_deg_ = 0.0;
    bool calibrating_ = false;
    bool connected_ = true;
};

class SimGps : public Gps {
public:
    GpsReading read() const override { return reading_; }
    void set_reading(const GpsReading& r) { reading_ = r; }

private:
    GpsReading reading_;
};

}  // namespace frostbyte::hal
