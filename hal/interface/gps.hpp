#pragma once

namespace frostbyte::hal {

struct GpsReading {
    bool valid = false;  // false: keine Messung oder Sensor ausgefallen
    double x_in = 0.0;   // Ursprung Feldmitte
    double y_in = 0.0;
    double heading_deg = 0.0;
    double error_in = 0.0;
};

class Gps {
public:
    virtual ~Gps() = default;

    virtual GpsReading read() const = 0;
};

}  // namespace frostbyte::hal
