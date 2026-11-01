#include "main.h"

#include "config/robot.hpp"
#include "core/log_format.hpp"  // prueft nur, dass der Log-v1-Schreiber fuer ARM kompiliert; genutzt ab F-05 (ADR-007)
#include "lemlib/api.hpp"  // prueft nur, dass LemLib einbindbar ist; Aufrufe erst hinter hal/ (ADR-001)
#include "hal/pros/pros_devices.hpp"  // prueft nur, dass die PROS-Wrapper kompilieren; instanziert wird erst mit den Ports (TODO(HW))

void initialize() {
	pros::lcd::initialize();
	pros::lcd::set_text(1, config::robot_name);
}

void disabled() {}

void competition_initialize() {}

void autonomous() {}

void opcontrol() {}
