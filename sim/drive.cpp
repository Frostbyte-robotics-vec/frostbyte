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
        throw std::runtime_error("sim_params " + where + "." + key + (allow_zero ? " darf nicht negativ sein"
                                                                                 : " muss positiv sein"));
    return v;
}

double sign(double v) { return v > 0.0 ? 1.0 : (v < 0.0 ? -1.0 : 0.0); }

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
    p.akku_V = number(r.at("akku"), "spannung_V", where + ".akku", false);
    p.g_in_s2 = number(params.at("welt"), "g_in_s2", "welt", false);
    return p;
}

DriveModel::DriveModel(const DriveParams& p, const Pose& start)
    : p_(p), x_in_(start.x_in), y_in_(start.y_in), h_rad_(start.h_deg * pi / 180.0) {}

void DriveModel::step_side(Side& s, double volts, double dt_s) const {
    const double v = s.v_in_s;
    if (v == 0.0 && std::abs(volts) <= p_.kS_V)
        return;  // Haftreibung haelt
    const double dir = v != 0.0 ? sign(v) : sign(volts);
    const double a_kennlinie = (volts - p_.kS_V * dir - p_.kV_V_per_in_s * v) / p_.kA_V_per_in_s2;
    const double a_max = p_.mu * p_.g_in_s2;  // mehr gibt die Haftung der Raeder nicht her (Schlupf)
    double next = v + std::clamp(a_kennlinie, -a_max, a_max) * dt_s;
    if (v != 0.0 && next * v < 0.0)
        next = 0.0;  // Vorzeichenwechsel: Reibung haelt an, kein Flattern um null
    s.v_in_s = next;
    s.dist_in += next * dt_s;
}

void DriveModel::step(double dt_s, double links_V, double rechts_V) {
    const double u_max = std::min(motor_max_V, p_.akku_V);
    step_side(links_, std::clamp(links_V, -u_max, u_max), dt_s);
    step_side(rechts_, std::clamp(rechts_V, -u_max, u_max), dt_s);

    // Halbimplizit wie die Seiten: neue Tempi, Heading in der Mitte des Schritts (keine Drift auf Kreisboegen).
    const double v_in_s = (links_.v_in_s + rechts_.v_in_s) / 2.0;
    const double dh_rad = (rechts_.v_in_s - links_.v_in_s) / p_.spurbreite_in * dt_s;
    const double h_mid = h_rad_ + dh_rad / 2.0;
    x_in_ += v_in_s * std::cos(h_mid) * dt_s;
    y_in_ += v_in_s * std::sin(h_mid) * dt_s;
    h_rad_ = std::remainder(h_rad_ + dh_rad, 2.0 * pi);
}

Pose DriveModel::pose() const {
    double h_deg = h_rad_ * 180.0 / pi;
    if (h_deg <= -180.0)
        h_deg += 360.0;
    return Pose{x_in_, y_in_, h_deg};
}

double DriveModel::heading_rate_deg_s() const {
    return (rechts_.v_in_s - links_.v_in_s) / p_.spurbreite_in * 180.0 / pi;
}

}  // namespace frostbyte::sim
