#pragma once

#include <string>

#include "sim/json.hpp"

// Fahrmodell der Simulation (SIM-02, ADR-008): Differentialantrieb, jede Seite mit der Kennlinie
// V = kS*sgn(v) + kV*v + kA*a am Rad (in/s), Schlupf als Beschleunigungsgrenze mu*g, Akku und Motor als
// Spannungsgrenze.
// Pose: Ursprung Feldmitte, 0 Grad = +x (rote -> blaue Station), Heading gegen den Uhrzeigersinn positiv.
namespace frostbyte::sim {

struct Pose {
    double x_in = 0.0;
    double y_in = 0.0;
    double h_deg = 0.0;
};

struct DriveParams {
    double kS_V = 0.0;
    double kV_V_per_in_s = 0.0;
    double kA_V_per_in_s2 = 0.0;
    double spurbreite_in = 0.0;
    double mu = 0.0;
    double motor_max_V = 0.0;  // hoechste Motorspannung, auch bei vollem Akku
    double akku_V = 0.0;
    double g_in_s2 = 0.0;
};

// Liest roboter.<robot>.antrieb, roboter.<robot>.akku.spannung_V und welt.g_in_s2. Fehlt etwas oder ist es nicht positiv
// (kS darf 0 sein), gibt es std::runtime_error.
DriveParams load_drive_params(const Json& params, const std::string& robot);

class DriveModel {
public:
    DriveModel(const DriveParams& p, const Pose& start);

    // Zeit um dt_s weiterschalten. Die Spannungen werden auf +-min(motor_max_V, akku_V) begrenzt.
    void step(double dt_s, double links_V, double rechts_V);

    // Wahre Pose; h_deg in (-180, 180].
    Pose pose() const;
    // Nur für Kollisionen (SIM-09): setzt die Position, die Blickrichtung bleibt. Der Anteil der Verschiebung in
    // Fahrtrichtung geht vom Radweg ab (festgehaltene Räder stehen, der Encoder zählt ihn nicht); Tempo gegen
    // die Verschiebung wird null (SIM-26). Seitliches Verschieben ändert Radweg und Tempo nicht.
    void set_position(double x_in, double y_in);
    // Nur für Feldobjekte (SIM-30): verschiebt die Pose, Tempo und Radweg bleiben (die Räder drehen weiter, der Roboter
    // rutscht gegen den Widerstand des Objekts). Anders als set_position kein Anhalten: ein Cup ist keine Wand.
    void displace(double dx_in, double dy_in);
    double heading_rate_deg_s() const;

    // Weg und Tempo je Seite am Boden (was ein Encoder an dieser Seite ohne Fehler saehe).
    double links_in() const { return links_.dist_in; }
    double rechts_in() const { return rechts_.dist_in; }
    double links_in_s() const { return links_.v_in_s; }
    double rechts_in_s() const { return rechts_.v_in_s; }

private:
    struct Side {
        double v_in_s = 0.0;
        double dist_in = 0.0;
    };

    void step_side(Side& s, double volts, double dt_s) const;

    DriveParams p_;
    double x_in_;
    double y_in_;
    double h_deg_;  // immer in (-180, 180]
    Side links_;
    Side rechts_;
};

}  // namespace frostbyte::sim
