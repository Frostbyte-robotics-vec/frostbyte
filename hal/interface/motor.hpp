#pragma once

// Schicht 5: Motor ohne Ports und Bibliotheken. Spannung in Volt, Winkel in Grad (nach dem Getriebe
// des Motors, noch nicht am Mechanismus), Zeit in Sekunden. Umrechnung von mV/RPM nur in hal/pros.
namespace frostbyte::hal {

class Motor {
public:
    virtual ~Motor() = default;

    // Wird auf den Spannungsbereich des Motors begrenzt (+-12 V).
    virtual void set_voltage_v(double volts) = 0;
    // Spannung 0 und bremsen (Matchende 0:00, RuleGuard).
    virtual void stop() = 0;

    virtual double position_deg() const = 0;
    virtual double velocity_deg_s() const = 0;
    virtual double temperature_c() const = 0;
    // false, wenn der Motor nicht am Port erkannt wird (SYS-12).
    virtual bool connected() const = 0;
};

}  // namespace frostbyte::hal
