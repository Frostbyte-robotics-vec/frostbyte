#include "sim/collision.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace frostbyte::sim {

namespace {

constexpr double pi = 3.14159265358979323846;
constexpr double touch_eps_in = 1e-12;

struct Frame {
    Vec2 u;  // Blickrichtung
    Vec2 v;  // quer dazu (links)
};

Frame frame_of(const Box& b) {
    const double h = b.h_deg * pi / 180.0;
    return Frame{{std::cos(h), std::sin(h)}, {-std::sin(h), std::cos(h)}};
}

std::array<Vec2, 4> corners_of(const Box& b) {
    const Frame f = frame_of(b);
    const double hl = b.length_in / 2.0;
    const double hw = b.width_in / 2.0;
    std::array<Vec2, 4> c{};
    int i = 0;
    for (const double su : {-1.0, 1.0})
        for (const double sv : {-1.0, 1.0})
            c[i++] = Vec2{b.x_in + su * hl * f.u.x_in + sv * hw * f.v.x_in,
                          b.y_in + su * hl * f.u.y_in + sv * hw * f.v.y_in};
    return c;
}

double dot(const Vec2& a, const Vec2& b) { return a.x_in * b.x_in + a.y_in * b.y_in; }

void project(const std::array<Vec2, 4>& c, const Vec2& axis, double& lo, double& hi) {
    lo = hi = dot(c[0], axis);
    for (const Vec2& p : c) {
        const double d = dot(p, axis);
        lo = std::min(lo, d);
        hi = std::max(hi, d);
    }
}

}  // namespace

std::optional<Vec2> penetration(const Box& a, const Box& b) {
    const auto ca = corners_of(a);
    const auto cb = corners_of(b);
    const Frame fa = frame_of(a);
    const Frame fb = frame_of(b);
    const std::array<Vec2, 4> axes{fa.u, fa.v, fb.u, fb.v};

    double best = 0.0;
    Vec2 best_axis;
    bool first = true;
    for (const Vec2& n : axes) {
        double lo_a, hi_a, lo_b, hi_b;
        project(ca, n, lo_a, hi_a);
        project(cb, n, lo_b, hi_b);
        const double overlap = std::min(hi_a, hi_b) - std::max(lo_a, lo_b);
        if (overlap <= touch_eps_in)
            return std::nullopt;
        if (first || overlap < best) {
            best = overlap;
            best_axis = n;
            first = false;
        }
    }
    // a muss von b weg: gegen die Richtung a -> b verschieben.
    const double side = dot(Vec2{b.x_in - a.x_in, b.y_in - a.y_in}, best_axis);
    const double sign = side >= 0.0 ? -1.0 : 1.0;
    return Vec2{sign * best * best_axis.x_in, sign * best * best_axis.y_in};
}

std::optional<Vec2> penetration(const Box& a, const Circle& c) {
    const Frame f = frame_of(a);
    const double hl = a.length_in / 2.0;
    const double hw = a.width_in / 2.0;
    const Vec2 rel{c.x_in - a.x_in, c.y_in - a.y_in};
    const double lu = dot(rel, f.u);  // Kreismitte im Boxsystem
    const double lv = dot(rel, f.v);
    const double du = lu - std::clamp(lu, -hl, hl);
    const double dv = lv - std::clamp(lv, -hw, hw);
    const double dist = std::sqrt(du * du + dv * dv);

    if (dist > 0.0) {
        if (dist >= c.r_in)
            return std::nullopt;
        const double k = -(c.r_in - dist) / dist;  // von der Kreismitte weg
        return Vec2{k * (du * f.u.x_in + dv * f.v.x_in), k * (du * f.u.y_in + dv * f.v.y_in)};
    }
    // Mittelpunkt in der Box: über die nächstgelegene Seite hinausschieben.
    const double to_u = hl - std::fabs(lu);
    const double to_v = hw - std::fabs(lv);
    if (to_u <= to_v) {
        const double push = -(lu >= 0.0 ? 1.0 : -1.0) * (to_u + c.r_in);
        return Vec2{push * f.u.x_in, push * f.u.y_in};
    }
    const double push = -(lv >= 0.0 ? 1.0 : -1.0) * (to_v + c.r_in);
    return Vec2{push * f.v.x_in, push * f.v.y_in};
}

Vec2 push_inside_field(const Box& a, double field_in) {
    const double half = field_in / 2.0;
    double min_x = 1e300, max_x = -1e300, min_y = 1e300, max_y = -1e300;
    for (const Vec2& p : corners_of(a)) {
        min_x = std::min(min_x, p.x_in);
        max_x = std::max(max_x, p.x_in);
        min_y = std::min(min_y, p.y_in);
        max_y = std::max(max_y, p.y_in);
    }
    Vec2 d;
    if (max_x > half)
        d.x_in = half - max_x;
    else if (min_x < -half)
        d.x_in = -half - min_x;
    if (max_y > half)
        d.y_in = half - max_y;
    else if (min_y < -half)
        d.y_in = -half - min_y;
    return d;
}

}  // namespace frostbyte::sim
