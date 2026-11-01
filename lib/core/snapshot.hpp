#pragma once

#include <cstdint>

#include "core/clock.hpp"

// Unveränderlicher Messwert-Satz eines Zyklus. Sensorwerte kommen mit den jeweiligen Tickets dazu.
namespace frostbyte {

struct Snapshot {
    std::uint32_t cycle = 0;
    double t_s = 0.0;
    double dt_s = 0.0;
};

class SnapshotBuilder {
public:
    explicit SnapshotBuilder(const Clock& clock) : clock_(clock), last_t_s_(clock.now_s()) {}

    Snapshot next() {
        Snapshot s;
        s.t_s = clock_.now_s();
        s.dt_s = s.t_s - last_t_s_;
        s.cycle = ++cycle_;
        last_t_s_ = s.t_s;
        return s;
    }

private:
    const Clock& clock_;
    double last_t_s_;
    std::uint32_t cycle_ = 0;
};

}  // namespace frostbyte
