#pragma once

#include <cstddef>
#include <cstdio>

#include "hal/interface/log_sink.hpp"

// Log-Datei auf der SD-Karte (PROS: Pfade unter /usd/). Header-only wie pros_storage.hpp (ADR-006). Jeder Lauf
// (Autonomie bzw. Fahrer) legt eine neue Datei /usd/log_<NNN>.csv an (aufsteigende freie Nummer; das Brain hat keine Uhr,
// ADR-047), nie wird eine alte überschrieben. begin_file()/append()/flush() nur im Logger-Task (und einmal in initialize()). TODO(HW): am Roboter prüfen:
// Karte fehlt (open_new false), Schreibpausen der Karte, Stromverlust während des Schreibens (fflush je Takt, kein fsync).
namespace frostbyte::hal {

class ProsLogSink : public LogSink {
public:
    static constexpr int max_files = 999;

    ~ProsLogSink() override {
        if (file_)
            std::fclose(file_);
    }

    // Schließt die bisherige Datei und legt die nächste freie an (Nummer nach der zuletzt vergebenen, dann weiter auf
    // Lücken prüfen). Rückgabe false, wenn keine Karte steckt oder alle Nummern belegt sind.
    bool begin_file() override {
        if (file_) {
            std::fclose(file_);
            file_ = nullptr;
        }
        while (last_ < max_files) {
            std::snprintf(path_, sizeof path_, "/usd/log_%03d.csv", ++last_);
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
    int last_ = 0;
    char path_[24] = "";
};

}  // namespace frostbyte::hal
