#pragma once

#include <cmath>

#include "core/units.hpp"

// Pose und Bogenschritt, gemeinsam für alle Odometrien (D-02, D-03).
// Konventionen wie im Feldplan: Zoll, Grad, 0° = +x (rote -> blaue Station), gegen den Uhrzeigersinn positiv.
namespace frostbyte {

struct Pose {
    double x_in = 0.0;
    double y_in = 0.0;
    double heading_deg = 0.0;  // (-180, 180]
};

// Bewegt die Pose um (dx, dy) im Roboterrahmen und dtheta_deg Drehung, alles aus einem Takt. Die Bewegung läuft auf einem
// Kreisbogen (Pose-Exponential), bezogen auf den Roboterrahmen am Takt-Anfang.
inline void advance_pose(Pose& pose, double dx_in, double dy_in, double dtheta_deg) {
    const double dtheta = deg_to_rad(dtheta_deg);
    double lx = dx_in, ly = dy_in;
    if (std::fabs(dtheta) > 1e-9) {
        const double s = std::sin(dtheta) / dtheta;
        const double c = (1.0 - std::cos(dtheta)) / dtheta;
        lx = dx_in * s - dy_in * c;
        ly = dx_in * c + dy_in * s;
    }
    const double h = deg_to_rad(pose.heading_deg);
    pose.x_in += lx * std::cos(h) - ly * std::sin(h);
    pose.y_in += lx * std::sin(h) + ly * std::cos(h);
    pose.heading_deg = wrap_deg(pose.heading_deg + dtheta_deg);
}

}  // namespace frostbyte
