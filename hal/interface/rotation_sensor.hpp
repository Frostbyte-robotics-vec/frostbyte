#pragma once

// Tracking-Rad bzw. Lift-/Armsensor. Winkel in Grad, aufsummiert (kein Umbruch bei 360).
namespace frostbyte::hal {

class RotationSensor {
public:
    virtual ~RotationSensor() = default;

    virtual double position_deg() const = 0;
    virtual double velocity_deg_s() const = 0;
    virtual void reset() = 0;
    virtual bool connected() const = 0;
};

}  // namespace frostbyte::hal
