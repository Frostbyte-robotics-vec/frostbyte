#include "sim/mechanism.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace frostbyte::sim {

namespace {

double clamp(double v, double lo, double hi) { return std::min(std::max(v, lo), hi); }

JointParams joint_from_json(const Json& j, const char* name) {
    JointParams p;
    p.min = j.at("min_deg").as_number();
    p.max = j.at("max_deg").as_number();
    p.max_vel = j.at("max_deg_s").as_number();
    p.max_acc = j.at("max_deg_s2").as_number();
    if (p.min > p.max || p.max_vel <= 0.0 || p.max_acc <= 0.0)
        throw std::runtime_error(std::string("sim_params mechanismen.") + name + ": Grenzen unplausibel");
    return p;
}

}  // namespace

Joint::Joint(const JointParams& p, double start_pos) : p_(p), pos_(clamp(start_pos, p.min, p.max)), target_(pos_) {}

void Joint::set_target(double target) { target_ = clamp(target, p_.min, p_.max); }

void Joint::step(double dt_s) {
    const double err = target_ - pos_;
    if (err == 0.0 && vel_ == 0.0)
        return;

    const double dir = err > 0.0 ? 1.0 : -1.0;
    // Gewünschte Geschwindigkeit: so schnell wie erlaubt, aber mit Bremsweg bis zum Ziel.
    double v_des = p_.max_vel;
    if (p_.max_acc > 0.0) {
        // Diskrete Bremskurve für Schritt dt: v so, dass v²/(2a) + v·dt/2 = Rest (sonst hinkt die Kurve im Raster nach).
        const double half = 0.5 * p_.max_acc * dt_s;
        v_des = std::min(v_des, std::sqrt(2.0 * p_.max_acc * std::fabs(err) + half * half) - half);
    }
    v_des *= dir;

    if (p_.max_acc > 0.0)
        vel_ += clamp(v_des - vel_, -p_.max_acc * dt_s, p_.max_acc * dt_s);
    else
        vel_ = v_des;

    const double next = pos_ + vel_ * dt_s;
    // Ziel erreicht oder überschritten: genau auf dem Ziel anhalten (kein Überschwingen).
    if ((target_ - pos_) * (target_ - next) <= 0.0) {
        pos_ = target_;
        vel_ = 0.0;
        return;
    }
    pos_ = next;
    if (pos_ <= p_.min || pos_ >= p_.max) {
        pos_ = clamp(pos_, p_.min, p_.max);
        vel_ = 0.0;
    }
}

double Joint::time_to_reach(double target) const {
    Joint copy = *this;
    copy.set_target(target);
    constexpr double dt = 0.001;
    constexpr int limit = 60000;
    for (int i = 0; i < limit; ++i) {
        if (copy.at_target())
            return i * dt;
        copy.step(dt);
    }
    return limit * dt;
}

Claw::Claw(const ClawParams& p) : p_(p), joint_(JointParams{0.0, p.open_in, p.max_in_s, 0.0}, p.open_in) {}

void Claw::command_open() { joint_.set_target(p_.open_in); }

void Claw::command_close(bool holding_pin) { joint_.set_target(holding_pin ? p_.pin_in : 0.0); }

double Claw::time_to_settle_after_close(bool holding_pin) const {
    return joint_.time_to_reach(holding_pin ? p_.pin_in : 0.0);
}

bool HeightMap::set(const std::array<double, 5>& angle_deg, const std::array<double, 5>& height_in) {
    return table_.set(angle_deg, height_in);
}

MechanismParams load_mechanism_params(const Json& params, const std::string& robot) {
    const Json& m = params.at("roboter").at(robot).at("mechanismen");
    MechanismParams out;
    out.lift = joint_from_json(m.at("lift"), "lift");
    if (const Json* h = m.at("lift").find("hoehe")) {  // optional (SIM-32): ohne sie ist die Höhe nicht modelliert
        std::array<double, 5> angle{}, height{};
        const Json& a = h->at("winkel_deg");
        const Json& v = h->at("hoehe_in");
        if (a.size() != 5 || v.size() != 5)
            throw std::runtime_error("sim_params mechanismen.lift.hoehe: genau 5 Stützpunkte nötig");
        for (std::size_t i = 0; i < 5; ++i) {
            angle[i] = a[i].as_number();
            height[i] = v[i].as_number();
        }
        if (!out.height.set(angle, height))
            throw std::runtime_error(
                "sim_params mechanismen.lift.hoehe: Werte nicht endlich oder nicht streng steigend");
    }
    if (const Json* arm = m.find("arm")) {
        out.has_arm = true;
        out.arm = joint_from_json(*arm, "arm");
    }
    const Json& c = m.at("claw");
    out.claw.open_in = c.at("offen_in").as_number();
    out.claw.pin_in = c.at("pin_in").as_number();
    out.claw.max_in_s = c.at("max_in_s").as_number();
    if (out.claw.pin_in < 0.0 || out.claw.pin_in > out.claw.open_in || out.claw.max_in_s <= 0.0)
        throw std::runtime_error("sim_params mechanismen.claw: Grenzen unplausibel");
    return out;
}

}  // namespace frostbyte::sim
