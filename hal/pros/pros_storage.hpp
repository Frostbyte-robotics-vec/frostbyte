#pragma once

#include <cstddef>
#include <cstdio>

#include "hal/interface/storage.hpp"

// SD-Karte (PROS: Pfade unter /usd/). Header-only wie pros_devices.hpp (ADR-006). Nur in initialize() lesen und im niedrig
// priorisierten Task schreiben, nie im Steuer-Task. TODO(HW): am Roboter pruefen: Karte steckt nicht -> fopen schlaegt
// fehl (missing/false), Schreiben und Lesen nach Neustart, Verhalten bei Stromverlust waehrend des Schreibens.
namespace frostbyte::hal {

class ProsStorage : public Storage {
public:
    explicit ProsStorage(const char* path) : path_(path) {}

    int read(char* buf, std::size_t n) const override {
        std::FILE* f = std::fopen(path_, "rb");
        if (!f)
            return -1;
        const std::size_t got = std::fread(buf, 1, n, f);
        const bool too_big = std::fgetc(f) != EOF;  // mehr Daten als n: nicht abgeschnitten liefern
        const bool err = std::ferror(f) != 0;
        std::fclose(f);
        return (too_big || err) ? -1 : static_cast<int>(got);
    }

    bool write(const char* data, std::size_t n) override {
        std::FILE* f = std::fopen(path_, "wb");
        if (!f)
            return false;
        const bool ok = std::fwrite(data, 1, n, f) == n;
        return (std::fclose(f) == 0) && ok;
    }

private:
    const char* path_;
};

}  // namespace frostbyte::hal
