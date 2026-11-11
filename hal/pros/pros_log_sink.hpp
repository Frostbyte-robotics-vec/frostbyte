#pragma once

#include <cstddef>
#include <cstdio>

#include "hal/interface/log_sink.hpp"

// Log-Datei auf der SD-Karte (PROS: Pfade unter /usd/). Header-only wie pros_storage.hpp (ADR-006). Jeder Start legt
// eine neue Datei /usd/log_<NNN>.csv an (kleinste freie Nummer; das Brain hat keine Uhr, ADR-047), nie wird eine alte
// überschrieben. open_new() in initialize(), append()/flush() nur im Logger-Task. TODO(HW): am Roboter prüfen:
// Karte fehlt (open_new false), Schreibpausen der Karte, Stromverlust während des Schreibens (fflush je Takt, kein fsync).
namespace frostbyte::hal {

class ProsLogSink : public LogSink {
public:
    static constexpr int max_files = 999;

    ~ProsLogSink() override {
        if (file_)
            std::fclose(file_);
    }

    // Legt die nächste freie Datei an. Rückgabe false, wenn keine Karte steckt oder alle Nummern belegt sind.
    bool open_new() {
        for (int n = 1; n <= max_files; ++n) {
            std::snprintf(path_, sizeof path_, "/usd/log_%03d.csv", n);
            if (std::FILE* probe = std::fopen(path_, "rb")) {
                std::fclose(probe);
                continue;
            }
            file_ = std::fopen(path_, "wb");
            return file_ != nullptr;
        }
        return false;
    }

    bool append(const char* data, std::size_t n) override { return file_ && std::fwrite(data, 1, n, file_) == n; }

    bool flush() override { return file_ && std::fflush(file_) == 0; }

    const char* path() const { return path_; }

private:
    std::FILE* file_ = nullptr;
    char path_[24] = "";
};

}  // namespace frostbyte::hal
