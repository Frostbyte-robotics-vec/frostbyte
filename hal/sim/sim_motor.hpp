#pragma once

#include <algorithm>
#include <cmath>

#include "hal/interface/motor.hpp"

// Fake-Motor mit Kennlinie: V = kS*sign(v) + kV*v + kA*a. Die Zahlen kommen aus sim/sim_params.json
// (lädt ein späteres SIM-Ticket); hier gibt es absichtlich keine Standardwerte.
// Strom: I = |V - kV*v| / R, begrenzt; kV*v ist die Gegenspannung. Blockiert (Stall) gilt v = 0.
namespace frostbyte::hal {

struct SimMotorParams {
    double kS_V;
    double kV_V_per_deg_s;
    double kA_V_per_deg_s2;
    double max_voltage_V;
    double resistance_ohm;  // Wicklung: I = (V - kV*v) / R
    double max_current_A;   // Begrenzung (V5-Motor schützt sich)
};

class SimMotor : public Motor {
public:
    explicit SimMotor(const SimMotorParams& p) : p_(p) {}

    void set_voltage_v(double volts) override {
        voltage_v_ = std::clamp(volts, -p_.max_voltage_V, p_.max_voltage_V);
        braking_ = false;
    }
    void stop() override {
        voltage_v_ = 0.0;
        braking_ = true;
    }

    double position_deg() const override { return position_deg_; }
    double velocity_deg_s() const override { return velocity_deg_s_; }
    double temperature_c() const override { return temperature_c_; }
    double current_a() const override {
        if (!connected_ || braking_)
            return 0.0;
        const double i = std::abs(voltage_v_ - p_.kV_V_per_deg_s * velocity_deg_s_) / p_.resistance_ohm;
        return std::min(i, p_.max_current_A);
    }
    bool connected() const override { return connected_; }

    // Nur Simulation: Zeit um dt_s weiterschalten (halbimplizites Euler-Verfahren).
    void step(double dt_s) {
        if (!connected_ || braking_ || blocked_) {
            velocity_deg_s_ = 0.0;
            return;
        }
        const double v = velocity_deg_s_;
        if (v == 0.0 && std::abs(voltage_v_) <= p_.kS_V)
            return;  // Haftreibung hält
        const double dir = v != 0.0 ? (v > 0.0 ? 1.0 : -1.0) : (voltage_v_ > 0.0 ? 1.0 : -1.0);
        const double accel = (voltage_v_ - p_.kS_V * dir - p_.kV_V_per_deg_s * v) / p_.kA_V_per_deg_s2;
        double next = v + accel * dt_s;
        if (v != 0.0 && next * v < 0.0)
            next = 0.0;  // Vorzeichenwechsel: Reibung hält an, kein Flattern um null
        velocity_deg_s_ = next;
        position_deg_ += velocity_deg_s_ * dt_s;
    }

    void set_connected(bool c) { connected_ = c; }
    // Nur Simulation: Mechanik blockiert (Stall); Geschwindigkeit bleibt 0, Spannung liegt weiter an.
    void set_blocked(bool b) { blocked_ = b; }

private:
    SimMotorParams p_;
    double voltage_v_ = 0.0;
    double position_deg_ = 0.0;
    double velocity_deg_s_ = 0.0;
    double temperature_c_ = 25.0;  // kein Temperaturmodell
    bool braking_ = false;
    bool blocked_ = false;
    bool connected_ = true;
};

}  // namespace frostbyte::hal
