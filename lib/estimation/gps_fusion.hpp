#pragma once

#include <cmath>

#include "core/units.hpp"
#include "estimation/pose.hpp"

// GPS-Fusion (D-04, ADR-016): zieht die Odometrie-Pose langsam zur GPS-Messung. Die Odometrie bleibt unverändert; die
// Fusion hält nur eine Korrektur (Versatz in Position und Heading), die auf jede Odometrie-Pose addiert wird.
// Das GPS-Sensor-Heading und die Einheiten müssen schon stimmen, bevor die Werte hier ankommen (HAL/config, TODO(HW)).
namespace frostbyte {

struct GpsReading {
    bool valid = false;  // false: in diesem Takt keine Messung (oder Aussetzer)
    double x_in = 0.0;
    double y_in = 0.0;
    double heading_deg = 0.0;
    double error_in = 0.0;  // vom Sensor gemeldete Güte (1 Sigma, Zoll); kleiner ist besser
};

struct GpsFusionParams {
    double max_error_in;    // gemeldete Güte schlechter als das: Messung verwerfen
    double max_speed_in_s;  // schnellere Fahrt: GPS verwerfen (Messung hinkt hinterher)
    double gate_in;         // weiter als das von der aktuellen Schätzung: Ausreißer verwerfen
    double position_gain;   // Anteil der Abweichung, der je angenommener Messung korrigiert wird (0..1)
    double heading_gain;    // dasselbe fürs Heading; 0 = Heading nur aus der Odometrie
};

enum class GpsResult { no_data, bad_quality, too_fast, outlier, accepted };

class GpsFusion {
public:
    explicit GpsFusion(const GpsFusionParams& p) : p_(p) {}

    // Löscht die Korrektur (neues Match, oder nach reset() der Odometrie).
    void reset() {
        dx_in_ = dy_in_ = dheading_deg_ = 0.0;
        result_ = GpsResult::no_data;
        pose_ = Pose{};
    }

    // Einmal je Steuertakt mit der Odometrie-Pose, der GPS-Messung dieses Takts (valid = false, wenn keine) und der
    // aktuellen Fahrgeschwindigkeit aus der Odometrie (Zoll/s).
    const Pose& update(const Pose& odo, const GpsReading& gps, double speed_in_s) {
        result_ = classify(odo, gps, speed_in_s);
        if (result_ == GpsResult::accepted) {
            dx_in_ += p_.position_gain * (gps.x_in - (odo.x_in + dx_in_));
            dy_in_ += p_.position_gain * (gps.y_in - (odo.y_in + dy_in_));
            dheading_deg_ += p_.heading_gain * wrap_deg(gps.heading_deg - (odo.heading_deg + dheading_deg_));
        }
        pose_ = Pose{odo.x_in + dx_in_, odo.y_in + dy_in_, wrap_deg(odo.heading_deg + dheading_deg_)};
        return pose_;
    }

    const Pose& pose() const { return pose_; }
    GpsResult last_result() const { return result_; }

private:
    GpsResult classify(const Pose& odo, const GpsReading& gps, double speed_in_s) const {
        if (!gps.valid)
            return GpsResult::no_data;
        if (!std::isfinite(gps.x_in) || !std::isfinite(gps.y_in) || !std::isfinite(gps.heading_deg) ||
            !std::isfinite(gps.error_in) || gps.error_in > p_.max_error_in)
            return GpsResult::bad_quality;
        if (std::fabs(speed_in_s) > p_.max_speed_in_s)
            return GpsResult::too_fast;
        if (std::hypot(gps.x_in - (odo.x_in + dx_in_), gps.y_in - (odo.y_in + dy_in_)) > p_.gate_in)
            return GpsResult::outlier;
        return GpsResult::accepted;
    }

    GpsFusionParams p_;
    double dx_in_ = 0.0;
    double dy_in_ = 0.0;
    double dheading_deg_ = 0.0;
    GpsResult result_ = GpsResult::no_data;
    Pose pose_;
};

}  // namespace frostbyte
