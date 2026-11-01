#include "main.h"

#include "config/robot.hpp"
#include "core/control_loop.hpp"
#include "core/log_format.hpp"  // prueft nur, dass der Log-v1-Schreiber fuer ARM kompiliert; genutzt ab F-05 (ADR-007)
#include "lemlib/api.hpp"  // prueft nur, dass LemLib einbindbar ist; Aufrufe erst hinter hal/ (ADR-001)
#include "hal/pros/pros_devices.hpp"  // prueft nur, dass die PROS-Wrapper kompilieren; instanziert wird erst mit den Ports (TODO(HW))

namespace {

constexpr std::uint32_t control_period_ms = 10;
constexpr std::uint32_t display_period_ms = 200;

// Alles statisch: der Steuer-Task legt nach dem Start nichts an.
frostbyte::PhaseFlag g_phase;
frostbyte::SharedCycleStats g_cycle_stats;

std::uint32_t micros_u32() { return static_cast<std::uint32_t>(pros::micros()); }

// Ein Steuer-Task fuer alle Phasen. Die Callbacks setzen nur g_phase, damit beim Uebergang
// Autonomie -> Fahrer kein Regler neu startet. Raster in millis() mit task_delay_until (kein Drift),
// Jitter aus micros()-Abstaenden. Nach einem Ueberzug wird das Raster uebersprungen statt nachgeholt.
void control_task(void*) {
	static frostbyte::ControlLoop loop(control_period_ms, pros::millis());
	frostbyte::Phase last_phase = g_phase.get();
	while (true) {
		loop.begin(micros_u32());
		const frostbyte::Phase phase = g_phase.get();
		if (phase != last_phase) {
			// Maxima je Phase: Ausreisser beim Start (initialize, LCD) sollen den Fahrbetrieb nicht ueberdecken.
			loop.reset_stats();
			last_phase = phase;
		}
		// TODO: Sensoren lesen, Odometrie, Logik, Regler, Motoren schreiben (je nach phase).
		// TODO(RuleGuard): in Phase Disabled alle Motoren auf Halt (Ticket mit Regelwaechter).
		loop.end(pros::millis(), micros_u32());
		g_cycle_stats.publish(loop.stats());
		std::uint32_t base_ms = frostbyte::delay_until_base_ms(loop.wake_ms(), control_period_ms);
		pros::c::task_delay_until(&base_ms, control_period_ms);
	}
}

// Zeigt die Takt-Statistik der aktuellen Phase auf dem LCD (Ablesen am Roboter, bis der Logger aus F-05 da ist).
// Eigener Task mit niedriger Prioritaet: die Ausgabe bleibt aus dem Steuer-Task heraus. Liest nur, setzt nie zurueck.
void display_task(void*) {
	std::uint32_t now_ms = pros::millis();
	while (true) {
		const frostbyte::CycleStatsView v = g_cycle_stats.load();
		pros::lcd::print(2, "jitter max %lu us", static_cast<unsigned long>(v.jitter_max_us));
		pros::lcd::print(3, "cpu max %lu us  ueberzuege %lu", static_cast<unsigned long>(v.work_max_us),
		                 static_cast<unsigned long>(v.overruns));
		pros::c::task_delay_until(&now_ms, display_period_ms);
	}
}

}  // namespace

void initialize() {
	pros::lcd::initialize();
	pros::lcd::set_text(1, config::robot_name);
	static pros::Task task(control_task, nullptr, TASK_PRIORITY_MAX - 1, TASK_STACK_DEPTH_DEFAULT, "control");
	static pros::Task display(display_task, nullptr, TASK_PRIORITY_MIN + 1, TASK_STACK_DEPTH_DEFAULT, "display");
}

void disabled() { g_phase.set(frostbyte::Phase::Disabled); }

void competition_initialize() { g_phase.set(frostbyte::Phase::Disabled); }

void autonomous() { g_phase.set(frostbyte::Phase::Autonomous); }

void opcontrol() { g_phase.set(frostbyte::Phase::Driver); }
