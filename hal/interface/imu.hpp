#pragma once

namespace frostbyte::hal {

class Imu {
public:
    virtual ~Imu() = default;

    // Grad in (-180, 180]. Das Vorzeichen gegenüber der Feldkonvention (0° = rot -> blau) ist am Roboter zu prüfen.
    virtual double heading_deg() const = 0;
    virtual bool calibrating() const = 0;
    virtual bool connected() const = 0;
};

}  // namespace frostbyte::hal
