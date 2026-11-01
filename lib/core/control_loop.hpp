#pragma once

#include <atomic>
#include <cstdint>

// Takt des Steuer-Tasks. Reine Logik ohne PROS: die Wartezeit selbst (delay_until) liegt in src/.
// Kein new, keine Ausgabe – alles Festgrößen, damit der Steuer-Task nach dem Start nichts anlegt.
// Ausnahme von „intern Sekunden“: hier wird direkt mit den Uhr-Ticks gerechnet (millis() fuer das Raster,
// micros() fuer die Messung), vorzeichenlos, damit der Ueberlauf der Uhren keine Rolle spielt.
namespace frostbyte {

// Die Competition-Callbacks setzen nur diese Phase; der Steuer-Task liest sie jeden Takt.
enum class Phase { Disabled, Autonomous, Driver };

class PhaseFlag {
public:
    void set(Phase p) { phase_.store(p, std::memory_order_relaxed); }
    Phase get() const { return phase_.load(std::memory_order_relaxed); }

private:
    std::atomic<Phase> phase_{Phase::Disabled};
};

// Naechster Rasterpunkt nach now_ms. Haengt nur vom Ursprung ab, nicht von der Rechenzeit (kein Drift).
// Ueberzogene Takte werden uebersprungen statt nachgeholt (kein Burst, der den naechsten Zyklus stoert);
// endet ein Takt genau auf einer Rastergrenze, ist dieser Punkt schon vorbei und der folgende gilt.
constexpr std::uint32_t next_wake_ms(std::uint32_t origin_ms, std::uint32_t now_ms, std::uint32_t period_ms) {
    const std::uint32_t slots = (now_ms - origin_ms) / period_ms + 1u;
    return origin_ms + slots * period_ms;
}

// task_delay_until(&base, period) weckt bei base + period. Liegt das schon in der Vergangenheit, kehrt es sofort
// zurueck und holt nach – deshalb wird die Basis jeden Takt aus next_wake_ms() neu gesetzt.
constexpr std::uint32_t delay_until_base_ms(std::uint32_t wake_ms, std::uint32_t period_ms) {
    return wake_ms - period_ms;
}

// Maxima und Ueberzuege seit dem letzten reset().
class CycleStats {
public:
    explicit CycleStats(std::uint32_t period_us) : period_us_(period_us) {}

    void record_jitter_us(std::uint32_t jitter_us) {
        if (jitter_us > jitter_max_us_) jitter_max_us_ = jitter_us;
    }

    void record_work_us(std::uint32_t work_us) {
        if (work_us > work_max_us_) work_max_us_ = work_us;
        if (work_us > period_us_) {
            ++overruns_;
            ++consecutive_;
        } else {
            consecutive_ = 0;
        }
    }

    // Die laufende Ueberzug-Folge bleibt: sie beschreibt den Zustand des Tasks, keine Statistik.
    void reset() {
        jitter_max_us_ = 0;
        work_max_us_ = 0;
        overruns_ = 0;
    }

    std::uint32_t jitter_max_us() const { return jitter_max_us_; }
    std::uint32_t work_max_us() const { return work_max_us_; }
    int overruns() const { return overruns_; }
    int consecutive_overruns() const { return consecutive_; }
    bool warning() const { return consecutive_ >= warn_after_; }

private:
    static constexpr int warn_after_ = 3;
    std::uint32_t period_us_;
    std::uint32_t jitter_max_us_ = 0;
    std::uint32_t work_max_us_ = 0;
    int overruns_ = 0;
    int consecutive_ = 0;
};

// Festes Raster in millis() (dieselbe Uhr wie task_delay_until). Der Jitter ist die Abweichung des mit micros()
// gemessenen Abstands zweier Takte von ihrem geplanten Rasterabstand – so steckt kein Versatz zwischen den beiden
// Uhren darin, und uebersprungene Takte zaehlen nicht als Jitter. Der erste Takt ist fuer start_ms angesetzt.
class ControlLoop {
public:
    ControlLoop(std::uint32_t period_ms, std::uint32_t start_ms)
        : period_ms_(period_ms), origin_ms_(start_ms), wake_ms_(start_ms), stats_(period_ms * 1000u) {}

    void begin(std::uint32_t now_us) {
        if (has_prev_) {
            const std::uint32_t actual_us = now_us - prev_begin_us_;
            const std::uint32_t planned_us = (wake_ms_ - prev_wake_ms_) * 1000u;
            stats_.record_jitter_us(actual_us > planned_us ? actual_us - planned_us : planned_us - actual_us);
        }
        has_prev_ = true;
        prev_begin_us_ = now_us;
        prev_wake_ms_ = wake_ms_;
    }

    void end(std::uint32_t now_ms, std::uint32_t now_us) {
        stats_.record_work_us(now_us - prev_begin_us_);
        wake_ms_ = next_wake_ms(origin_ms_, now_ms, period_ms_);
    }

    // Auch der Vorgaenger wird vergessen: der Abstand zu einem Ausreisser gehoert noch zur alten Phase.
    void reset_stats() {
        stats_.reset();
        has_prev_ = false;
    }

    std::uint32_t wake_ms() const { return wake_ms_; }
    const CycleStats& stats() const { return stats_; }

private:
    std::uint32_t period_ms_;
    std::uint32_t origin_ms_;
    std::uint32_t wake_ms_;
    std::uint32_t prev_wake_ms_ = 0;
    std::uint32_t prev_begin_us_ = 0;
    bool has_prev_ = false;
    CycleStats stats_;
};

struct CycleStatsView {
    std::uint32_t jitter_max_us = 0;
    std::uint32_t work_max_us = 0;
    std::uint32_t overruns = 0;
};

// Uebergabe an den Anzeige-Task: nur der Steuer-Task schreibt, andere Tasks lesen nur.
// Ganzzahlige Atomics, weil atomic<double> auf dem V5 nicht sicher lock-free ist. Die Felder sind einzeln
// konsistent, nicht als Satz – fuer eine Anzeige reicht das.
class SharedCycleStats {
public:
    void publish(const CycleStats& s) {
        jitter_max_us_.store(s.jitter_max_us(), std::memory_order_relaxed);
        work_max_us_.store(s.work_max_us(), std::memory_order_relaxed);
        overruns_.store(static_cast<std::uint32_t>(s.overruns()), std::memory_order_relaxed);
    }

    CycleStatsView load() const {
        CycleStatsView v;
        v.jitter_max_us = jitter_max_us_.load(std::memory_order_relaxed);
        v.work_max_us = work_max_us_.load(std::memory_order_relaxed);
        v.overruns = overruns_.load(std::memory_order_relaxed);
        return v;
    }

private:
    std::atomic<std::uint32_t> jitter_max_us_{0};
    std::atomic<std::uint32_t> work_max_us_{0};
    std::atomic<std::uint32_t> overruns_{0};
};

}  // namespace frostbyte
