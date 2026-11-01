#pragma once

// Zeitquelle. Der Roboter liefert sie aus pros::millis() (hal/pros), Tests und Simulation aus FakeClock.
namespace frostbyte {

class Clock {
public:
    virtual ~Clock() = default;
    virtual double now_s() const = 0;
};

class FakeClock : public Clock {
public:
    double now_s() const override { return t_s_; }
    void advance_s(double dt_s) { t_s_ += dt_s; }

private:
    double t_s_ = 0.0;
};

}  // namespace frostbyte
