#include "main.h"

#include "config/robot.hpp"
#include "lemlib/api.hpp"  // prueft nur, dass LemLib einbindbar ist; Aufrufe erst hinter hal/ (ADR-001)

void initialize() {
	pros::lcd::initialize();
	pros::lcd::set_text(1, config::robot_name);
}

void disabled() {}

void competition_initialize() {}

void autonomous() {}

void opcontrol() {}
