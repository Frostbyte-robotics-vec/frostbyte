#pragma once

#include <algorithm>
#include <cmath>

// Schicht 4: Regler (D-05, ADR-017). Fehler in der Einheit der Regelgröße (Zoll oder Grad), Ausgabe in Volt, dt in Sekunden.
namespace frostbyte {

// Schnittstelle, damit der MotionController den eigenen Pid oder einen fremden Regler (LemLib, src/) nutzen kann.
class Regulator {
public:
    virtual ~Regulator() = default;
    virtual double update(double error, double dt_s) = 0;
    virtual void reset() = 0;
};

struct PidGains {
    double kp = 0.0;          // V je Fehlereinheit
    double ki = 0.0;          // V je (Fehlereinheit * s)
    double kd = 0.0;          // V je (Fehlereinheit / s)
    double i_zone = 0.0;      // nur integrieren, wenn |Fehler| < i_zone; 0 = immer
    double i_max_v = 12.0;    // Grenze des I-Anteils
    double out_max_v = 12.0;  // Grenze der Ausgabe
};

// PID mit Begrenzung und Anti-Windup: I-Anteil begrenzt, keine Integration, solange die Ausgabe in Fehlerrichtung
// gesättigt ist. D-Anteil auf den Fehler, beim ersten Aufruf nach reset() ohne Sprung. Ungültiger Fehler → 0 V.
class Pid : public Regulator {
public:
    explicit Pid(const PidGains& g) : g_(g) {}

    double update(double error, double dt_s) override {
        if (!std::isfinite(error))
            return 0.0;
        const bool timed = dt_s > 0.0 && std::isfinite(dt_s);
        double d_term = 0.0;
        if (timed && has_prev_)
            d_term = g_.kd * (error - prev_error_) / dt_s;
        prev_error_ = error;
        has_prev_ = true;

        double i_term = i_term_;
        if (timed && (g_.i_zone <= 0.0 || std::fabs(error) < g_.i_zone))
            i_term = std::clamp(i_term_ + g_.ki * error * dt_s, -g_.i_max_v, g_.i_max_v);

        const double raw = g_.kp * error + i_term + d_term;
        const bool saturated_with_error = std::fabs(raw) > g_.out_max_v && (raw > 0.0) == (error > 0.0);
        if (!saturated_with_error)
            i_term_ = i_term;
        const double out = g_.kp * error + i_term_ + d_term;
        return std::clamp(out, -g_.out_max_v, g_.out_max_v);
    }

    // Verstärkungen im Betrieb ändern (Tuning-Modus, D-06). Der aufgelaufene I-Anteil ist schon mit ki multipliziert
    // und bleibt, springt also nicht; eine engere I-Grenze kappt ihn sofort.
    void set_gains(const PidGains& g) {
        g_ = g;
        i_term_ = std::clamp(i_term_, -g_.i_max_v, g_.i_max_v);
    }

    void reset() override {
        i_term_ = 0.0;
        prev_error_ = 0.0;
        has_prev_ = false;
    }

private:
    PidGains g_;
    double i_term_ = 0.0;  // schon mit ki multipliziert, damit eine Änderung von ki nicht springt
    double prev_error_ = 0.0;
    bool has_prev_ = false;
};

}  // namespace frostbyte
