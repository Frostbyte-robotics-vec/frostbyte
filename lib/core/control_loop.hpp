#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>

// Takt des Steuer-Tasks. Reine Logik ohne PROS: die Wartezeit selbst (delay_until) liegt in src/.
// Kein new, keine Ausgabe – alles Festgrößen, damit der Steuer-Task nach dem Start nichts anlegt.
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

// Misst Jitter (Abweichung des tatsaechlichen vom angeforderten Weckzeitpunkt) und Rechenzeit je Zyklus.
class CycleStats {
public:
    explicit CycleStats(double period_s) : period_s_(period_s) {}

    void tick_start(double now_s, double scheduled_s) {
        jitter_max_s_ = std::max(jitter_max_s_, std::abs(now_s - scheduled_s));
        start_s_ = now_s;
    }

    void tick_end(double now_s) {
        const double work_s = now_s - start_s_;
        work_max_s_ = std::max(work_max_s_, work_s);
        if (work_s > period_s_) {
            ++overruns_;
            ++consecutive_;
        } else {
            consecutive_ = 0;
        }
    }

    double jitter_max_s() const { return jitter_max_s_; }
    double work_max_s() const { return work_max_s_; }
    int overruns() const { return overruns_; }
    int consecutive_overruns() const { return consecutive_; }
    bool warning() const { return consecutive_ >= warn_after_; }

private:
    static constexpr int warn_after_ = 3;
    double period_s_;
    double start_s_ = 0.0;
    double jitter_max_s_ = 0.0;
    double work_max_s_ = 0.0;
    int overruns_ = 0;
    int consecutive_ = 0;
};

// Festes Raster: der naechste Weckzeitpunkt haengt nur vom Start ab, nicht von der Rechenzeit (kein Drift).
// Der erste Takt ist fuer start_s angesetzt.
class ControlLoop {
public:
    ControlLoop(double period_s, double start_s)
        : period_s_(period_s), origin_s_(start_s), next_wake_s_(start_s), stats_(period_s) {}

    void begin(double now_s) { stats_.tick_start(now_s, next_wake_s_); }
    void end(double now_s) {
        stats_.tick_end(now_s);
        // Ueberzogene Takte werden uebersprungen statt nachgeholt (kein Burst, der den naechsten Zyklus stoert).
        // Kleine Toleranz, damit ein Takt, der exakt auf der Rastergrenze endet, nicht faelschlich ausfaellt.
        const double slots = std::floor((now_s - origin_s_) / period_s_ + 1e-9) + 1.0;
        next_wake_s_ = origin_s_ + slots * period_s_;
    }

    double next_wake_s() const { return next_wake_s_; }
    const CycleStats& stats() const { return stats_; }

private:
    double period_s_;
    double origin_s_;
    double next_wake_s_;
    CycleStats stats_;
};

}  // namespace frostbyte
