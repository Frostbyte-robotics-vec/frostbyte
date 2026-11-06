#pragma once

#include <string>
#include <vector>

#include "hal/interface/controller_output.hpp"

// Controller-Ausgabe für Tests: merkt sich, was gesendet wurde.
namespace frostbyte::hal {

class SimControllerOutput : public ControllerOutput {
public:
    struct Print {
        int line;
        std::string text;
    };

    bool rumble(const char* pattern) override {
        rumbles.emplace_back(pattern);
        return true;
    }

    bool print(int line, const char* text) override {
        if (!accept_prints)
            return false;
        prints.push_back({line, text});
        return true;
    }

    std::vector<std::string> rumbles;
    std::vector<Print> prints;
    bool accept_prints = true;  // false: der Controller lehnt Text ab (Test für Wiederholung)
};

}  // namespace frostbyte::hal
