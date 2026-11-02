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

// Die Header der Roboter liegen in config::stacker bzw. config::runner (beide gleichzeitig einbindbar, z. B. in Tests);
// hier wird der gewählte Roboter nach config:: übernommen.
namespace config {
#if defined(FROSTBYTE_ROBOT_STACKER)
using namespace stacker;
#else
using namespace runner;
#endif
}  // namespace config
