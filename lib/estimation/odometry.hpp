#pragma once

#include <cmath>

#include "core/units.hpp"

// Odometrie Stacker (D-02): zwei Tracking-Räder (parallel und quer) plus IMU-Heading.
// Konventionen wie im Feldplan: Zoll, Grad, 0° = +x (rote -> blaue Station), gegen den Uhrzeigersinn positiv.
// Das IMU-Vorzeichen (HAL/config, TODO(HW)) muss schon stimmen, bevor der Wert hier ankommt.
namespace frostbyte {

struct Pose {
    double x_in = 0.0;
    double y_in = 0.0;
    double heading_deg = 0.0;  // (-180, 180]
};

// Lage der Räder im Roboterkoordinatensystem (x nach vorn, y nach links), vom Drehmittelpunkt aus gemessen.
struct TrackingGeometry {
    double parallel_y_in = 0.0;       // Parallelrad (rollt in x-Richtung): seitlicher Versatz, links positiv
    double perpendicular_x_in = 0.0;  // Querrad (rollt in y-Richtung): Versatz nach vorn, vorn positiv
};

class TrackingOdometry {
public:
    explicit TrackingOdometry(const TrackingGeometry& g) : g_(g) {}

    // Setzt Pose und Bezugswerte. Die Wegsummen und das IMU-Heading sind die Rohwerte dieses Moments;
    // ein IMU-Versatz zum Feld fällt dadurch heraus (nur Änderungen des IMU-Winkels zählen).
    void reset(const Pose& pose, double parallel_total_in, double perpendicular_total_in, double imu_heading_deg) {
        pose_ = pose;
        pose_.heading_deg = wrap_deg(pose.heading_deg);
        last_par_in_ = parallel_total_in;
        last_perp_in_ = perpendicular_total_in;
        last_imu_deg_ = imu_heading_deg;
    }

    // Einmal je Steuertakt mit den aktuellen Summen (Weg seit beliebigem Nullpunkt, vorzeichenbehaftet).
    void update(double parallel_total_in, double perpendicular_total_in, double imu_heading_deg) {
        const double dtheta_deg = wrap_deg(imu_heading_deg - last_imu_deg_);
        const double dtheta = deg_to_rad(dtheta_deg);
        // Ein Rad abseits der Drehmitte misst bei Drehung Bogenlänge mit; sie wird abgezogen.
        const double dx = (parallel_total_in - last_par_in_) + dtheta * g_.parallel_y_in;
        const double dy = (perpendicular_total_in - last_perp_in_) - dtheta * g_.perpendicular_x_in;

        // Bewegung auf einem Kreisbogen (Pose-Exponential) im Roboterrahmen am Takt-Anfang.
        double lx = dx, ly = dy;
        if (std::fabs(dtheta) > 1e-9) {
            const double s = std::sin(dtheta) / dtheta;
            const double c = (1.0 - std::cos(dtheta)) / dtheta;
            lx = dx * s - dy * c;
            ly = dx * c + dy * s;
        }
        const double h = deg_to_rad(pose_.heading_deg);
        pose_.x_in += lx * std::cos(h) - ly * std::sin(h);
        pose_.y_in += lx * std::sin(h) + ly * std::cos(h);
        pose_.heading_deg = wrap_deg(pose_.heading_deg + dtheta_deg);

        last_par_in_ = parallel_total_in;
        last_perp_in_ = perpendicular_total_in;
        last_imu_deg_ = imu_heading_deg;
    }

    const Pose& pose() const { return pose_; }

private:
    TrackingGeometry g_;
    Pose pose_;
    double last_par_in_ = 0.0;
    double last_perp_in_ = 0.0;
    double last_imu_deg_ = 0.0;
};

}  // namespace frostbyte
