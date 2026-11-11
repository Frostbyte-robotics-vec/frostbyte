#pragma once

#include <cstddef>
#include <string>

#include "hal/interface/log_sink.hpp"

// Log-Ziel im Arbeitsspeicher für Tests (kein Dateizugriff, gleiche Eingabe = gleiche Bytes).
namespace frostbyte::hal {

class MemoryLogSink : public LogSink {
public:
    bool append(const char* data, std::size_t n) override {
        if (fail_writes_)
            return false;
        data_.append(data, n);
        return true;
    }

    bool flush() override {
        ++flushes_;
        return !fail_writes_;
    }

    void set_fail_writes(bool f) { fail_writes_ = f; }
    const std::string& contents() const { return data_; }
    int flushes() const { return flushes_; }

private:
    std::string data_;
    bool fail_writes_ = false;
    int flushes_ = 0;
};

}  // namespace frostbyte::hal
