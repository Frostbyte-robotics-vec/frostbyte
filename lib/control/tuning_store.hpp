#pragma once

#include "control/tuning.hpp"
#include "hal/interface/storage.hpp"

// Laden und Speichern der Tuning-Werte über hal::Storage (D-06, ADR-020). Getrennt von tuning.hpp, damit der
// Tuning-Modus im Steuer-Task nichts vom Speicher weiß. Laden: einmal in initialize(). Speichern: nur im niedrig
// priorisierten Task, wenn TuningMode::take_save_request() true war, nie im 10-ms-Steuer-Task.
namespace frostbyte {

// Bei missing oder invalid bleibt `values` unverändert (Standardwerte aus config/).
inline LoadResult load_tuning(const hal::Storage& store, TuningSet& values) {
    char buf[512];
    const int n = store.read(buf, sizeof(buf));
    if (n < 0)
        return LoadResult::missing;
    return parse_tuning(buf, static_cast<std::size_t>(n), values) ? LoadResult::loaded : LoadResult::invalid;
}

inline bool save_tuning(hal::Storage& store, const TuningSet& values) {
    char buf[512];
    const int n = serialize_tuning(values, buf, sizeof(buf));
    return n > 0 && store.write(buf, static_cast<std::size_t>(n));
}

}  // namespace frostbyte
