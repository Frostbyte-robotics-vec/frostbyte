#include "sim/sensor_models.hpp"

#include <cmath>
#include <stdexcept>

namespace frostbyte::sim {

double gauss(Rng& rng) {
    double sum = 0.0;
    for (int i = 0; i < 12; ++i)
        sum += rng.uniform();
    return sum - 6.0;
}

namespace {

// Bringt Grad in (-180, 180] (gleiche Konvention wie hal::Imu).
double wrap_heading(double deg) {
    deg = std::fmod(deg, 360.0);
    if (deg > 180.0)
        deg -= 360.0;
    else if (deg <= -180.0)
        deg += 360.0;
    return deg;
}

double non_negative(const Json& obj, const char* key) {
    const double v = obj.at(key).as_number();
    if (v < 0.0)
        throw std::runtime_error(std::string("sim_params: ") + key + " darf nicht negativ sein");
    return v;
}

enum StreamId : std::uint64_t { odometry_id = 1, imu_id = 2, gps_id = 3 };

// Jedes Modell bekommt seinen eigenen Strom: Seed und Kennzahl, gemischt über splitmix64.
Rng stream(std::uint64_t seed, StreamId id) { return Rng(Rng(seed ^ (id * 0xD1B54A32D192ED03ull)).next()); }

}  // namespace

SensorParams load_sensor_params(const Json& params, const std::string& robot, const std::string& profile) {
    SensorParams out;
    out.noise_scale = non_negative(params.at("rauschen_profile").at(profile), "skala");

    const Json& s = params.at("roboter").at(robot).at("sensoren");
    const Json* odo = s.find("tracking");
    out.odometry_is_tracking = odo != nullptr;
    if (!odo)
        odo = &s.at("encoder");
    out.odometry.scale_error = non_negative(*odo, "skalierung_fehler");
    out.odometry.sigma_in_per_tick = non_negative(*odo, "sigma_in_pro_takt");

    out.imu.drift_deg_per_min = non_negative(s.at("imu"), "drift_deg_pro_min");
    out.imu.sigma_deg = non_negative(s.at("imu"), "sigma_deg");

    const Json& g = s.at("gps");
    out.gps.sigma_in = non_negative(g, "sigma_in");
    out.gps.sigma_deg = non_negative(g, "sigma_deg");
    out.gps.dropout_fraction = non_negative(g, "aussetzer_anteil");
    out.gps.quality_loss_per_in_s = non_negative(g, "guete_verlust_pro_in_s");
    if (out.gps.dropout_fraction * out.noise_scale > 1.0)
        throw std::runtime_error("sim_params gps: aussetzer_anteil mal Rauschskala liegt über 1");
    return out;
}

OdometryModel::OdometryModel(const OdometryParams& p, double noise_scale, std::uint64_t seed)
    : rng_(stream(seed, odometry_id)), sigma_(p.sigma_in_per_tick * noise_scale) {
    const double sign = rng_.uniform() < 0.5 ? -1.0 : 1.0;
    scale_ = 1.0 + sign * p.scale_error * noise_scale;
}

void OdometryModel::step(double true_delta_in) { measured_in_ += true_delta_in * scale_ + sigma_ * gauss(rng_); }

ImuModel::ImuModel(const ImuParams& p, double noise_scale, std::uint64_t seed)
    : rng_(stream(seed, imu_id)), sigma_deg_(p.sigma_deg * noise_scale) {
    const double sign = rng_.uniform() < 0.5 ? -1.0 : 1.0;
    drift_deg_s_ = sign * p.drift_deg_per_min * noise_scale / 60.0;
}

double ImuModel::measure_deg(double true_heading_deg, double t_s) {
    return wrap_heading(true_heading_deg + drift_deg_s_ * t_s + sigma_deg_ * gauss(rng_));
}

GpsModel::GpsModel(const GpsParams& p, double noise_scale, std::uint64_t seed) : rng_(stream(seed, gps_id)), p_(p) {
    p_.sigma_in *= noise_scale;
    p_.sigma_deg *= noise_scale;
    p_.dropout_fraction *= noise_scale;
}

GpsSample GpsModel::measure(double true_x_in, double true_y_in, double true_heading_deg, double speed_in_s) {
    // Immer gleich viele Zufallszahlen je Aufruf, damit ein Aussetzer die Folge der übrigen Messungen nicht verschiebt.
    const double drop = rng_.uniform();
    const double nx = gauss(rng_);
    const double ny = gauss(rng_);
    const double nh = gauss(rng_);

    GpsSample s;
    if (drop < p_.dropout_fraction)
        return s;
    const double sigma = p_.sigma_in * (1.0 + p_.quality_loss_per_in_s * std::fabs(speed_in_s));
    s.valid = true;
    s.x_in = true_x_in + sigma * nx;
    s.y_in = true_y_in + sigma * ny;
    s.heading_deg = wrap_heading(true_heading_deg + p_.sigma_deg * nh);
    s.error_in = sigma;
    return s;
}

}  // namespace frostbyte::sim
