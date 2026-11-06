#pragma once

#include <array>
#include <string>

#include "sim/json.hpp"
#include "util/monotone_table.hpp"

// Mechanismen der Simulation als Gelenke (SIM-04): Lift, Arm und Greifer folgen einem Sollwert mit
// Geschwindigkeits- und Beschleunigungsgrenze und stoßen an Endlagen an. Alle Zahlen aus sim_params.json
// (roboter.<name>.mechanismen). Einheit der Position steht im Namen des Aufrufers (Grad bzw. Zoll beim Greifer).
namespace frostbyte::sim {

struct JointParams {
    double min = 0.0;
    double max = 0.0;
    double max_vel = 0.0;  // je Sekunde
    double max_acc = 0.0;  // je Sekunde²; 0 = keine Beschleunigungsgrenze (Geschwindigkeit springt)
};

class Joint {
public:
    explicit Joint(const JointParams& p, double start_pos = 0.0);

    // Sollwert wird auf [min, max] begrenzt.
    void set_target(double target);
    double target() const { return target_; }
    double pos() const { return pos_; }
    double vel() const { return vel_; }
    bool at_target() const { return pos_ == target_ && vel_ == 0.0; }
    bool at_limit() const { return pos_ <= p_.min || pos_ >= p_.max; }

    // Ein Schritt von dt_s Sekunden.
    void step(double dt_s);

    // Zeit in Sekunden, bis eine Kopie dieses Gelenks den Sollwert erreicht (Schritt 1 ms, Abbruch nach 60 s -> 60).
    double time_to_reach(double target) const;

private:
    JointParams p_;
    double pos_;
    double vel_ = 0.0;
    double target_;
};

// Greifer: ein Gelenk in Zoll Öffnungsweite. Greift er ein Teil, bleibt er bei dessen Breite stehen.
struct ClawParams {
    double open_in = 0.0;
    double pin_in = 0.0;  // Öffnungsweite um einen gegriffenen Pin
    double max_in_s = 0.0;
};

class Claw {
public:
    explicit Claw(const ClawParams& p);

    void command_open();
    void command_close(bool holding_pin);  // leer: schließt ganz (0); mit Pin: bis pin_in
    void step(double dt_s) { joint_.step(dt_s); }
    double width_in() const { return joint_.pos(); }
    bool settled() const { return joint_.at_target(); }
    double time_to_settle_after_close(bool holding_pin) const;

private:
    ClawParams p_;
    Joint joint_;
};

// Höhe des Roboters (Oberkante) über dem Lift-Winkel (SIM-32): 5 Stützpunkte, monotone Interpolation wie am Roboter
// (lib/util/monotone_table.hpp). Ohne gültige Tabelle ist die Höhe nicht modelliert; SG3 wird dann nicht geprüft.
class HeightMap {
public:
    // Winkel und Höhe streng steigend und endlich; sonst false und ungültig.
    bool set(const std::array<double, 5>& angle_deg, const std::array<double, 5>& height_in);
    bool valid() const { return table_.valid(); }
    double at(double angle_deg) const { return table_.eval(angle_deg); }

private:
    MonotoneTable<5> table_;
};

struct MechanismParams {
    JointParams lift;
    HeightMap height;
    bool has_arm = false;  // nur der Stacker hat einen Arm
    JointParams arm;
    ClawParams claw;
};

// Liest roboter.<robot>.mechanismen. Fehlende Schlüssel oder min > max: std::runtime_error.
MechanismParams load_mechanism_params(const Json& params, const std::string& robot);

}  // namespace frostbyte::sim
