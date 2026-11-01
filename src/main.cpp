#include "main.h"

#include "config/robot.hpp"
#include "core/control_loop.hpp"
#include "lemlib/api.hpp"  // prueft nur, dass LemLib einbindbar ist; Aufrufe erst hinter hal/ (ADR-001)
#include "hal/pros/pros_devices.hpp"  // prueft nur, dass die PROS-Wrapper kompilieren; instanziert wird erst mit den Ports (TODO(HW))

namespace {

constexpr std::uint32_t control_period_ms = 10;

// Alles statisch: der Steuer-Task legt nach dem Start nichts an.
frostbyte::PhaseFlag g_phase;

// Ein Steuer-Task fuer alle Phasen. Die Callbacks setzen nur g_phase, damit beim Uebergang
// Autonomie -> Fahrer kein Regler neu startet. Taktung mit task_delay_until (kein driftender Takt);
// Jitter wird mit micros() gemessen. Nach einem Ueberzug wird das Raster uebersprungen statt nachgeholt.
void control_task(void*) {
	// Raster ms-ausgerichtet starten: task_delay_until weckt auf ms-Ticks, sonst steckt ein fester Versatz im Jitter.
	std::uint32_t wake_ms = pros::millis();
	static frostbyte::ControlLoop loop(control_period_ms / 1000.0, wake_ms / 1000.0);
	while (true) {
		loop.begin(pros::micros() / 1e6);
		// TODO: Sensoren lesen, Odometrie, Logik, Regler, Motoren schreiben (je nach g_phase.get()).
		// TODO(RuleGuard): in Phase Disabled alle Motoren auf Halt (Ticket mit Regelwaechter).
		loop.end(pros::micros() / 1e6);
		// Rasterzeit des Loops (micros) -> naechster Wecker in ms, task_delay_until addiert die Periode selbst.
		const std::uint32_t next_ms = static_cast<std::uint32_t>(loop.next_wake_s() * 1000.0 + 0.5);
		wake_ms = next_ms - control_period_ms;
		pros::c::task_delay_until(&wake_ms, control_period_ms);
	}
}

}  // namespace

void initialize() {
	pros::lcd::initialize();
	pros::lcd::set_text(1, config::robot_name);
	static pros::Task task(control_task, nullptr, TASK_PRIORITY_MAX - 1, TASK_STACK_DEPTH_DEFAULT, "control");
}

void disabled() { g_phase.set(frostbyte::Phase::Disabled); }

void competition_initialize() { g_phase.set(frostbyte::Phase::Disabled); }

void autonomous() { g_phase.set(frostbyte::Phase::Autonomous); }

void opcontrol() { g_phase.set(frostbyte::Phase::Driver); }
