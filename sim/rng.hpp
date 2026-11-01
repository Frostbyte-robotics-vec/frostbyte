#pragma once

#include <cstdint>

namespace frostbyte::sim {

// splitmix64: auf allen Plattformen gleiche Folge.
class Rng {
public:
    explicit Rng(std::uint64_t seed) : state_(seed) {}
    std::uint64_t next();
    // Gleichverteilt in [0, 1).
    double uniform();

private:
    std::uint64_t state_;
};

}  // namespace frostbyte::sim
