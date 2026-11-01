#pragma once

#include <cstdint>
#include <string>

#include "sim/json.hpp"
#include "sim/world.hpp"

// Sensormodelle der Simulation (SIM-03): machen aus wahren Größen die Messwerte, die der Roboter sieht.
// Alle Zahlen kommen aus sim_params.json (roboter.<name>.sensoren), das Rauschprofil (none/realistic/harsh) skaliert
// Fehler, Rauschen, Drift und Aussetzer gemeinsam. Zufall nur über sim::Rng; jedes Modell hat einen eigenen Strom
// (Seed + Kennzahl), das Ergebnis hängt also nicht von der Aufrufreihenfolge der Modelle ab.
namespace frostbyte::sim {

// Näherungsweise N(0, 1): Summe von 12 Gleichverteilten minus 6. Nur Grundrechenarten, daher auf jeder Plattform
// bit-gleich (kein log/cos wie bei Box-Muller). Werte liegen in [-6, 6].
double gauss(Rng& rng);

// Tracking-Rad (Stacker) bzw. Antriebsencoder (Runner): Wegmessung mit festem Skalierungsfehler und Rauschen je Takt.
struct OdometryParams {
    double scale_error = 0.0;  // Betrag, Vorzeichen wird je Lauf gezogen (0.005 = 0,5 %)
    double sigma_in_per_tick = 0.0;
};

struct ImuParams {
    double drift_deg_per_min = 0.0;  // Betrag, Vorzeichen wird je Lauf gezogen
    double sigma_deg = 0.0;          // Rauschen je Messung
};

struct GpsParams {
    double sigma_in = 0.0;               // Positionsrauschen im Stand
    double sigma_deg = 0.0;              // Rauschen der Blickrichtung
    double dropout_fraction = 0.0;       // Anteil ungültiger Messungen
    double quality_loss_per_in_s = 0.0;  // Güteverlust je Zoll/Sekunde Fahrgeschwindigkeit
};

struct SensorParams {
    OdometryParams odometry;
    bool odometry_is_tracking = false;  // true: "tracking" (Tracking-Rad), false: "encoder"
    ImuParams imu;
    GpsParams gps;
    double noise_scale = 1.0;
};

// Liest roboter.<robot>.sensoren und rauschen_profile.<profile>.skala. Fehlende Schlüssel: std::runtime_error.
SensorParams load_sensor_params(const Json& params, const std::string& robot, const std::string& profile);

class OdometryModel {
public:
    OdometryModel(const OdometryParams& p, double noise_scale, std::uint64_t seed);

    // Wahrer Weg dieses Takts (vorzeichenbehaftet) -> aufsummierte Messung.
    void step(double true_delta_in);
    double measured_in() const { return measured_in_; }
    double scale() const { return scale_; }  // 1 ± Skalierungsfehler dieses Laufs

private:
    Rng rng_;
    double scale_;
    double sigma_;
    double measured_in_ = 0.0;
};

class ImuModel {
public:
    ImuModel(const ImuParams& p, double noise_scale, std::uint64_t seed);

    // Wahre Blickrichtung zur Zeit t_s -> Messwert in (-180, 180]: Wahrheit + Drift * t + Rauschen.
    double measure_deg(double true_heading_deg, double t_s);
    double drift_deg_per_s() const { return drift_deg_s_; }

private:
    Rng rng_;
    double drift_deg_s_;
    double sigma_deg_;
};

struct GpsSample {
    bool valid = false;
    double x_in = 0.0;
    double y_in = 0.0;
    double heading_deg = 0.0;
    double error_in = 0.0;  // gemeldete Güte (1 Sigma), wächst mit der Fahrgeschwindigkeit
};

class GpsModel {
public:
    GpsModel(const GpsParams& p, double noise_scale, std::uint64_t seed);

    GpsSample measure(double true_x_in, double true_y_in, double true_heading_deg, double speed_in_s);

private:
    Rng rng_;
    GpsParams p_;
};

}  // namespace frostbyte::sim
