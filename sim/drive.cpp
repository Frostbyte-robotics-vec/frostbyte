#include "sim/drive.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace frostbyte::sim {

namespace {

constexpr double pi = 3.14159265358979323846;

double number(const Json& obj, const char* key, const std::string& where, bool allow_zero) {
    const double v = obj.at(key).as_number();
    if (v < 0.0 || (!allow_zero && v == 0.0))
        throw std::runtime_error("sim_params " + where + "." + key +
                                 (allow_zero ? " darf nicht negativ sein" : " muss positiv sein"));
    return v;
}

double sign(double v) { return v > 0.0 ? 1.0 : (v < 0.0 ? -1.0 : 0.0); }

// Grad in (-180, 180].
double wrap_deg(double deg) {
    double w = std::remainder(deg, 360.0);
    if (w <= -180.0)
        w += 360.0;
    return w;
}

}  // namespace

DriveParams load_drive_params(const Json& params, const std::string& robot) {
    const Json& r = params.at("roboter").at(robot);
    const Json& a = r.at("antrieb");
    const std::string where = "roboter." + robot;
    DriveParams p;
    p.kS_V = number(a, "kS_V", where + ".antrieb", true);
    p.kV_V_per_in_s = number(a, "kV_V_per_in_s", where + ".antrieb", false);
    p.kA_V_per_in_s2 = number(a, "kA_V_per_in_s2", where + ".antrieb", false);
    p.spurbreite_in = number(a, "spurbreite_in", where + ".antrieb", false);
    p.mu = number(a, "mu", where + ".antrieb", false);
    p.motor_max_V = number(a, "motor_max_V", where + ".antrieb", false);
    p.akku_V = number(r.at("akku"), "spannung_V", where + ".akku", false);
    p.g_in_s2 = number(params.at("welt"), "g_in_s2", "welt", false);
    return p;
}

DriveModel::DriveModel(const DriveParams& p, const Pose& start)
    : p_(p), x_in_(start.x_in), y_in_(start.y_in), h_deg_(wrap_deg(start.h_deg)) {}

void DriveModel::step_side(Side& s, double volts_V, double dt_s) const {
    const double v_in_s = s.v_in_s;
    if (v_in_s == 0.0 && std::abs(volts_V) <= p_.kS_V)
        return;  // Haftreibung haelt
    const double dir = v_in_s != 0.0 ? sign(v_in_s) : sign(volts_V);
    const double a_kennlinie_in_s2 = (volts_V - p_.kS_V * dir - p_.kV_V_per_in_s * v_in_s) / p_.kA_V_per_in_s2;
    const double a_max_in_s2 = p_.mu * p_.g_in_s2;  // mehr gibt die Haftung der Raeder nicht her (Schlupf)
    double v_next_in_s = v_in_s + std::clamp(a_kennlinie_in_s2, -a_max_in_s2, a_max_in_s2) * dt_s;
    if (v_in_s != 0.0 && v_next_in_s * v_in_s < 0.0)
        v_next_in_s = 0.0;  // Vorzeichenwechsel: Reibung haelt an, kein Flattern um null
    s.v_in_s = v_next_in_s;
    s.dist_in += v_next_in_s * dt_s;
}

void DriveModel::step(double dt_s, double links_V, double rechts_V) {
    const double u_max_V = std::min(p_.motor_max_V, p_.akku_V);
    step_side(links_, std::clamp(links_V, -u_max_V, u_max_V), dt_s);
    step_side(rechts_, std::clamp(rechts_V, -u_max_V, u_max_V), dt_s);

    // Halbimplizit wie die Seiten: neue Tempi, Heading in der Mitte des Schritts (keine Drift auf Kreisboegen).
    // Bogenmass nur hier fuer cos/sin; der Zustand bleibt in Grad.
    const double v_in_s = (links_.v_in_s + rechts_.v_in_s) / 2.0;
    const double dh_deg = heading_rate_deg_s() * dt_s;
    const double h_mid_rad = (h_deg_ + dh_deg / 2.0) * pi / 180.0;
    x_in_ += v_in_s * std::cos(h_mid_rad) * dt_s;
    y_in_ += v_in_s * std::sin(h_mid_rad) * dt_s;
    h_deg_ = wrap_deg(h_deg_ + dh_deg);
}

void DriveModel::set_position(double x_in, double y_in) {
    const double h_rad = h_deg_ * pi / 180.0;
    const double along_in = (x_in - x_in_) * std::cos(h_rad) + (y_in - y_in_) * std::sin(h_rad);
    for (Side* s : {&links_, &rechts_}) {
        s->dist_in += along_in;
        if (s->v_in_s * along_in < 0.0)
            s->v_in_s = 0.0;
    }
    x_in_ = x_in;
    y_in_ = y_in;
}

void DriveModel::displace(double dx_in, double dy_in) {
    const double h_rad = h_deg_ * pi / 180.0;
    const double along_in = dx_in * std::cos(h_rad) + dy_in * std::sin(h_rad);
    for (Side* s : {&links_, &rechts_})
        s->dist_in += along_in;
    x_in_ += dx_in;
    y_in_ += dy_in;
}

Pose DriveModel::pose() const { return Pose{x_in_, y_in_, h_deg_}; }

double DriveModel::heading_rate_deg_s() const {
    return (rechts_.v_in_s - links_.v_in_s) / p_.spurbreite_in * 180.0 / pi;
}

}  // namespace frostbyte::sim
