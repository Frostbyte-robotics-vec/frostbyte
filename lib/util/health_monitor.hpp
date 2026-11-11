#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>

#include "hal/interface/motor.hpp"

// Health-Monitor (F-06, ADR-048): prüft im Health-Task (100 ms) Motoren (getrennt, heiß), Akku und die Takt-Warnung des
// Steuer-Tasks und meldet die wichtigste Warnung. Reine Logik ohne PROS, ohne dynamischen Speicher. Der Monitor wirkt nie
// auf Motoren; die Reaktion (Strom senken, Subsystem auf FEHLER) gehört den Subsystemen. Er meldet nur: Text für den
// Controller (match/controller_feedback) und ein Ereignis-Token für das Log.
namespace frostbyte {

// Startwerte (Software-Entscheidung), am Roboter prüfen: TODO(HW) in config/. Hysterese: eine Warnung geht erst unter der
// Rückstellschwelle weg, damit sie nicht flackert.
struct HealthConfig {
    double temp_warn_c = 55.0;  // V5-Motoren drosseln ab etwa 55 °C
    double temp_clear_c = 50.0;
    double battery_warn_v = 11.0;
    double battery_clear_v = 11.5;
    int lost_polls =
        2;  // Messungen in Folge ohne Motor, bevor gewarnt wird (bei 100 ms: Warnung nach höchstens 200 ms)
};

// Je höher der Wert, desto wichtiger die Warnung.
enum class HealthWarning : std::uint8_t { none, cycle_overrun, battery_low, motor_hot, motor_lost };

struct HealthStatus {
    HealthWarning warning = HealthWarning::none;
    int motor_index = -1;  // bei motor_hot / motor_lost: erster betroffener Motor in der Reihenfolge von add_motor()
    bool any() const { return warning != HealthWarning::none; }
};

struct HealthInput {
    double battery_v = std::numeric_limits<double>::quiet_NaN();  // NaN = nicht bekannt, wird nicht bewertet
    bool cycle_warning = false;                                   // CycleStats::warning() des Steuer-Tasks
};

template <std::size_t MaxMotors>
class HealthMonitor {
public:
    explicit HealthMonitor(const HealthConfig& cfg) : cfg_(cfg) {}

    // Beim Start aufrufen (vor dem Health-Task). name muss so lange gültig sein wie der Monitor (Stringliteral).
    // false, wenn kein Platz mehr ist.
    bool add_motor(const hal::Motor& motor, const char* name) {
        if (count_ >= MaxMotors)
            return false;
        motors_[count_++] = Entry{&motor, name, 0, false};
        return true;
    }

    // Health-Task, alle 100 ms.
    HealthStatus update(const HealthInput& in) {
        HealthStatus out;
        for (std::size_t i = 0; i < count_; ++i) {
            Entry& e = motors_[i];
            if (!e.motor->connected()) {
                if (e.missing_polls < cfg_.lost_polls)
                    ++e.missing_polls;
                continue;  // Temperatur eines getrennten Motors ist bedeutungslos
            }
            e.missing_polls = 0;
            const double t = e.motor->temperature_c();
            if (std::isfinite(t)) {
                if (t >= cfg_.temp_warn_c)
                    e.hot = true;
                else if (t < cfg_.temp_clear_c)
                    e.hot = false;
            }
        }
        for (std::size_t i = 0; i < count_; ++i)
            if (motors_[i].missing_polls >= cfg_.lost_polls) {
                out.warning = HealthWarning::motor_lost;
                out.motor_index = static_cast<int>(i);
                break;
            }
        if (!out.any())
            for (std::size_t i = 0; i < count_; ++i)
                if (motors_[i].hot && motors_[i].missing_polls == 0) {
                    out.warning = HealthWarning::motor_hot;
                    out.motor_index = static_cast<int>(i);
                    break;
                }
        if (std::isfinite(in.battery_v)) {
            if (in.battery_v <= cfg_.battery_warn_v)
                battery_low_ = true;
            else if (in.battery_v > cfg_.battery_clear_v)
                battery_low_ = false;
        }
        if (!out.any() && battery_low_)
            out.warning = HealthWarning::battery_low;
        if (!out.any() && in.cycle_warning)
            out.warning = HealthWarning::cycle_overrun;
        return out;
    }

    // Kurzer Text für die Controller-Zeile (leer = alles gut). Immer nullterminiert, bei kleinem Puffer abgeschnitten.
    const char* text(const HealthStatus& s, char* buf, std::size_t n) const {
        switch (s.warning) {
            case HealthWarning::motor_lost: return put(buf, n, std::snprintf(buf, n, "AUS %s", name_of(s)));
            case HealthWarning::motor_hot: return put(buf, n, std::snprintf(buf, n, "HEISS %s", name_of(s)));
            case HealthWarning::battery_low: return put(buf, n, std::snprintf(buf, n, "AKKU NIEDRIG"));
            case HealthWarning::cycle_overrun: return put(buf, n, std::snprintf(buf, n, "TAKT"));
            case HealthWarning::none: break;
        }
        return put(buf, n, 0);
    }

    // Ereignis-Token für das Feld event im Log (docs/log_format.md); leer, wenn keine Warnung.
    const char* event_token(const HealthStatus& s, char* buf, std::size_t n) const {
        switch (s.warning) {
            case HealthWarning::motor_lost:
                return put(buf, n, std::snprintf(buf, n, "HEALTH:MOTOR_LOST:%s", name_of(s)));
            case HealthWarning::motor_hot: return put(buf, n, std::snprintf(buf, n, "HEALTH:MOTOR_HOT:%s", name_of(s)));
            case HealthWarning::battery_low: return put(buf, n, std::snprintf(buf, n, "HEALTH:BATTERY_LOW"));
            case HealthWarning::cycle_overrun: return put(buf, n, std::snprintf(buf, n, "HEALTH:CYCLE_OVERRUN"));
            case HealthWarning::none: break;
        }
        return put(buf, n, 0);
    }

private:
    struct Entry {
        const hal::Motor* motor = nullptr;
        const char* name = "";
        int missing_polls = 0;
        bool hot = false;
    };

    const char* name_of(const HealthStatus& s) const {
        return (s.motor_index >= 0 && static_cast<std::size_t>(s.motor_index) < count_) ? motors_[s.motor_index].name
                                                                                        : "?";
    }

    // Nach snprintf(buf, n, ...): bei n == 0 gibt es keinen Puffer, sonst ist buf nullterminiert (auch bei Abschneiden).
    // written == 0 mit n > 0 heißt: leerer Text (snprintf wurde nicht aufgerufen).
    static const char* put(char* buf, std::size_t n, int written) {
        if (n > 0 && written == 0)
            buf[0] = '\0';
        return buf;
    }

    HealthConfig cfg_;
    std::array<Entry, MaxMotors> motors_{};
    std::size_t count_ = 0;
    bool battery_low_ = false;
};

// Übergabe Health-Task -> andere Tasks (Steuer-Task für das Log, Controller-Task für die Anzeige): nur der Health-Task
// schreibt. Ganzzahlige Atomics wie SharedCycleStats; Warnung und Index sind einzeln konsistent, für eine Anzeige reicht das.
class SharedHealth {
public:
    void publish(const HealthStatus& s) {
        index_.store(static_cast<std::int8_t>(s.motor_index), std::memory_order_relaxed);
        warning_.store(static_cast<std::uint8_t>(s.warning), std::memory_order_relaxed);
    }

    HealthStatus load() const {
        HealthStatus s;
        s.warning = static_cast<HealthWarning>(warning_.load(std::memory_order_relaxed));
        s.motor_index = index_.load(std::memory_order_relaxed);
        return s;
    }

private:
    std::atomic<std::uint8_t> warning_{0};
    std::atomic<std::int8_t> index_{-1};
};

}  // namespace frostbyte
