#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "hal/interface/log_sink.hpp"

// Log-Ziel im Arbeitsspeicher für Tests (kein Dateizugriff, gleiche Eingabe = gleiche Bytes).
namespace frostbyte::hal {

class MemoryLogSink : public LogSink {
public:
    bool begin_file() override {
        if (fail_writes_)
            return false;
        files_.emplace_back();
        return true;
    }

    bool append(const char* data, std::size_t n) override {
        if (fail_writes_)
            return false;
        if (files_.empty())
            files_.emplace_back();  // ohne begin_file(): alles in einer Datei
        files_.back().append(data, n);
        return true;
    }

    bool flush() override {
        ++flushes_;
        return !fail_writes_;
    }

    void set_fail_writes(bool f) { fail_writes_ = f; }
    // Alle Dateien hintereinander (wie ein einzelner Lauf, wenn nur eine Datei entstand).
    std::string contents() const {
        std::string all;
        for (const std::string& f : files_)
            all += f;
        return all;
    }
    const std::vector<std::string>& files() const { return files_; }
    int flushes() const { return flushes_; }

private:
    std::vector<std::string> files_;
    bool fail_writes_ = false;
    int flushes_ = 0;
};

}  // namespace frostbyte::hal
