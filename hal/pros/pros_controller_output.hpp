#pragma once

#include "api.h"
#include "hal/interface/controller_output.hpp"

// Fahrer-Controller (PROS). Header-only wie pros_devices.hpp (ADR-006). Nur im niedrig priorisierten Task benutzen.
// TODO(HW): am Controller prüfen: Textzeile erscheint und flackert nicht bei 10 Hz, kurzes und langes Muster sind
// unterscheidbar, nichts passiert bei getrenntem Controller (PROS gibt dann einen Fehlerwert zurück, kein Absturz).
namespace frostbyte::hal {

class ProsControllerOutput : public ControllerOutput {
public:
    bool rumble(const char* pattern) override { return controller_.rumble(pattern) == 1; }

    bool print(int line, const char* text) override { return controller_.set_text(line, 0, text) == 1; }

private:
    pros::Controller controller_{pros::E_CONTROLLER_MASTER};
};

}  // namespace frostbyte::hal
