#pragma once

#include <array>
#include <cstddef>
#include <cstring>

#include "hal/interface/storage.hpp"

// Speicher im Arbeitsspeicher für Tests. Die Simulation liest nie eine Datei von der Platte (gleicher Seed = gleiches
// Ergebnis, CLAUDE.md); Tuning-Dateien spielen dort keine Rolle.
namespace frostbyte::hal {

class SimStorage : public Storage {
public:
    static constexpr std::size_t capacity = 1024;

    int read(char* buf, std::size_t n) const override {
        if (!has_data_ || size_ > n)
            return -1;
        std::memcpy(buf, data_.data(), size_);
        return static_cast<int>(size_);
    }

    bool write(const char* data, std::size_t n) override {
        if (fail_writes_ || n > capacity)
            return false;
        std::memcpy(data_.data(), data, n);
        size_ = n;
        has_data_ = true;
        return true;
    }

    void set_fail_writes(bool f) { fail_writes_ = f; }

private:
    std::array<char, capacity> data_{};
    std::size_t size_ = 0;
    bool has_data_ = false;
    bool fail_writes_ = false;
};

}  // namespace frostbyte::hal
