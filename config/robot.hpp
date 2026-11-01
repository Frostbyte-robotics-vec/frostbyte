#pragma once

// Waehlt die Konfiguration des Zielroboters. Das Define setzt das Makefile (make ROBOT=stacker | runner).
// Unterschiede zwischen den Robotern stehen nur in config/stacker.hpp und config/runner.hpp.
#if defined(FROSTBYTE_ROBOT_STACKER)
#include "config/stacker.hpp"
#elif defined(FROSTBYTE_ROBOT_RUNNER)
#include "config/runner.hpp"
#else
#error "Kein Zielroboter: make ROBOT=stacker oder make ROBOT=runner"
#endif
