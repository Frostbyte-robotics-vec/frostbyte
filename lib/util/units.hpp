#pragma once

#include <cmath>

// Intern: Zoll, Grad, Sekunden. Umrechnung nur an den Rändern (Motoren, Bibliotheken).
namespace frostbyte {

inline constexpr double pi = 3.14159265358979323846;

constexpr double deg_to_rad(double deg) { return deg * pi / 180.0; }
constexpr double rad_to_deg(double rad) { return rad * 180.0 / pi; }

// Winkel in Grad nach (-180, 180]
inline double wrap_deg(double deg) {
    double w = std::fmod(deg, 360.0);
    if (w > 180.0)
        w -= 360.0;
    if (w <= -180.0)
        w += 360.0;
    return w;
}

}  // namespace frostbyte
