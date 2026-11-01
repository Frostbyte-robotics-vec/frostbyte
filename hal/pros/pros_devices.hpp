#pragma once

#include <cmath>
#include <cstdint>

#include "api.h"
#include "core/units.hpp"
#include "hal/interface/gps.hpp"
#include "hal/interface/imu.hpp"
#include "hal/interface/motor.hpp"
#include "hal/interface/rotation_sensor.hpp"

// PROS-Implementierung der Geräteschnittstellen. Header-only, weil das Makefile nur src/ kompiliert (ADR-006).
// Ports gehören in config/ (TODO(HW)); hier wird nichts instanziert. Alle PROS-Einheiten (mV, RPM,
// Zentigrad, Meter) werden nur in dieser Datei umgerechnet.
namespace frostbyte::hal {

class ProsMotor : public Motor {
public:
    // Negativer Port dreht die Richtung um (PROS 4). Position in Grad am Motorausgang des Getriebeeinsatzes.
    ProsMotor(std::int8_t port, pros::v5::MotorGears gearset) : motor_(port, gearset) {
        motor_.set_encoder_units(pros::v5::MotorUnits::degrees);
        motor_.set_brake_mode(pros::v5::MotorBrake::hold);
    }

    void set_voltage_v(double volts) override {
        motor_.move_voltage(static_cast<std::int32_t>(std::lround(volts * 1000.0)));
    }
    void stop() override {
        motor_.move_voltage(0);
        motor_.brake();
    }

    double position_deg() const override { return motor_.get_position(); }
    double velocity_deg_s() const override { return motor_.get_actual_velocity() * 6.0; }  // RPM -> Grad/s
    double temperature_c() const override { return motor_.get_temperature(); }
    bool connected() const override { return motor_.is_installed(); }

private:
    mutable pros::Motor motor_;
};

class ProsRotation : public RotationSensor {
public:
    explicit ProsRotation(std::int8_t port) : sensor_(port) {}

    double position_deg() const override { return sensor_.get_position() / 100.0; }
    double velocity_deg_s() const override { return sensor_.get_velocity() / 100.0; }
    void reset() override { sensor_.reset_position(); }
    bool connected() const override { return sensor_.is_installed(); }

private:
    mutable pros::Rotation sensor_;
};

class ProsImu : public Imu {
public:
    explicit ProsImu(std::uint8_t port) : imu_(port) {}

    // TODO(HW): PROS zählt Heading 0..360 im Uhrzeigersinn; Vorzeichen zur Feldkonvention am Roboter prüfen.
    double heading_deg() const override { return wrap_deg(imu_.get_heading()); }
    bool calibrating() const override { return imu_.is_calibrating(); }
    bool connected() const override { return imu_.is_installed(); }

private:
    mutable pros::Imu imu_;
};

class ProsGps : public Gps {
public:
    explicit ProsGps(std::uint8_t port) : gps_(port) {}

    // TODO(HW): Ursprung, Achsrichtung und Heading-Bezug des GPS gegen die Feldkonvention prüfen.
    GpsReading read() const override {
        GpsReading r;
        if (!gps_.is_installed())
            return r;
        const double err_m = gps_.get_error();
        const auto pos = gps_.get_position();
        const double heading = gps_.get_heading();
        if (!std::isfinite(err_m) || !std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(heading))
            return r;
        r.valid = true;
        r.x_in = mm_to_in(pos.x * 1000.0);
        r.y_in = mm_to_in(pos.y * 1000.0);
        r.heading_deg = wrap_deg(heading);
        r.error_in = mm_to_in(err_m * 1000.0);
        return r;
    }

private:
    mutable pros::Gps gps_;
};

}  // namespace frostbyte::hal
