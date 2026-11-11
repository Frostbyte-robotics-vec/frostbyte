#include "main.h"

#include "config/robot.hpp"
#include "control/motion_controller.hpp"
#include "control/tuning_store.hpp"
#include "core/control_loop.hpp"
#include "core/logger.hpp"                      // Log v1: Schlange Steuer-Task -> Logger-Task -> SD (F-05, ADR-047)
#include "hal/pros/pros_controller_output.hpp"  // Vibration und Anzeige am Controller (C-04)
#include "hal/pros/pros_devices.hpp"  // prueft nur, dass die PROS-Wrapper kompilieren; instanziert wird erst mit den Ports (TODO(HW))
#include "hal/pros/pros_log_sink.hpp"  // Log-Datei auf der SD-Karte (F-05)
#include "hal/pros/pros_storage.hpp"   // SD-Karte fuer die Tuning-Werte (D-06)
#include "lemlib/api.hpp"              // prueft nur, dass LemLib einbindbar ist; Aufrufe erst hinter hal/ (ADR-001)
#include "lemlib_regulator.hpp"
#include "link_comms.hpp"
#include "link_test.hpp"
#include "match/controller_feedback.hpp"
#include "match/routine_screen.hpp"
#include "port_check.hpp"
#include "util/build_info.hpp"
#include "util/rule_guard.hpp"

static_assert(!(config::port_check_enabled && config::link_test_enabled),
              "Pruefprogramme (F-03, L-01) nie gleichzeitig");
static_assert(!(config::link_enabled && (config::link_test_enabled || config::port_check_enabled)),
              "Funk im Match (L-02) nie zusammen mit einem Pruefprogramm: gleicher Funk-Port");

namespace {

constexpr std::uint32_t control_period_ms = 10;
constexpr std::uint32_t display_period_ms = 200;
constexpr std::uint32_t feedback_period_ms = 100;
constexpr double driver_run_s = 60.0;  // Skills: ein Fahrerlauf dauert 60 s
constexpr std::uint32_t menu_period_ms = 100;
constexpr std::uint32_t logger_period_ms = 50;  // docs/plan.md: Logger-Task 50 ms, niedrige Prioritaet

// Alles statisch: der Steuer-Task legt nach dem Start nichts an.
frostbyte::PhaseFlag g_phase;
frostbyte::SharedCycleStats g_cycle_stats;

// Bewegung (D-05, ADR-017): eigener Pid wie in der Simulation; lemlib::PID nur, wenn config es verlangt (Vergleich D-07).
// Der Steuer-Task ruft g_motion.update() auf, sobald Pose (F-05) und Routen (A-03) da sind.
frostbyte::Pid g_lat_pid(config::lateral_pid);
frostbyte::Pid g_ang_pid(config::angular_pid);
frostbyte::LemlibRegulator g_lat_lemlib(config::lateral_pid, control_period_ms / 1000.0);
frostbyte::LemlibRegulator g_ang_lemlib(config::angular_pid, control_period_ms / 1000.0);
frostbyte::Regulator& lateral_regulator() {
    if constexpr (config::motion_uses_lemlib_pid)
        return g_lat_lemlib;
    return g_lat_pid;
}
frostbyte::Regulator& angular_regulator() {
    if constexpr (config::motion_uses_lemlib_pid)
        return g_ang_lemlib;
    return g_ang_pid;
}
frostbyte::MotionController g_motion(config::motion, lateral_regulator(), angular_regulator());

// Tuning (D-06, ADR-020): nur mit config::tuning_enabled und eigenem Pid. Die Werte werden einmal in initialize() von der
// SD-Karte gelesen. TODO(D-09): TuningMode im Steuer-Task mit dem Controller fuettern, take_save_request() im niedrig
// priorisierten Task mit save_tuning() beantworten und take_event() ins Log schreiben (braucht Logger F-05); Fahrer-Aktionen sperren.
frostbyte::hal::ProsStorage g_tuning_store("/usd/tuning.txt");

// Routinenmenue (A-11, ADR-030): Auswahl auf der SD-Karte, geladen in initialize(), gespeichert nur im UI-Task (menu_task),
// nie im Steuer-Task. TODO(A-12): autonomous() liest g_routine_screen.selected() und startet die Route (braucht A-10).
frostbyte::hal::ProsStorage g_routine_store("/usd/routine.txt");
frostbyte::RoutineScreen g_routine_screen(frostbyte::make_routine_menu());

// RuleGuard (M-07, ADR-027): letzter Filter vor den Motoren. Noch nicht verdrahtet (Lift, Antrieb und Greifer haben noch keine
// Ports, TODO(HW)); der Steuer-Task ruft g_rule_guard.filter() auf, sobald Motoren geschrieben werden, und gibt
// Restzeit/Besitz/Lift-Zustand hinein. Disabled -> Halt bleibt Sache dieses Aufrufs (siehe TODO im Steuer-Task).
frostbyte::RuleGuard g_rule_guard(config::rule_guard);

// Release-Stand (L-03): Defines kommen aus dem Makefile; fehlen sie (z. B. IDE-Build), steht "?" bzw. "-" da.
#ifndef FB_VERSION
#define FB_VERSION "?"
#endif
#ifndef FB_COMMIT
#define FB_COMMIT ""
#endif
#ifndef FB_DIRTY
#define FB_DIRTY 1
#endif

// Log (F-05, ADR-047): Der Steuer-Task kopiert alle 20 ms eine Zeile in die Schlange (nie schreiben, nie blockieren),
// der Logger-Task (niedrige Prioritaet) schreibt sie auf die SD-Karte. 256 Zeilen = ca. 5 s Puffer, wenn die Karte
// stockt; danach faellt die neueste Zeile weg und die naechste meldet LOGDROP:<n>. Eine Datei je Lauf (/usd/log_NNN.csv).
// TODO(HW): Schreibpausen der SD-Karte messen (Puffer gross genug?). Gefuellt sind erst t_s, robot, cpu_ms und Phase;
// Pose, Mechanismen usw. kommen mit den Tickets, die sie im Steuer-Task berechnen (A-10, M-10, F-06).
frostbyte::hal::ProsLogSink g_log_sink;
frostbyte::Logger<256> g_log(config::robot_name, FB_COMMIT[0] ? FB_COMMIT : "-");

std::uint32_t micros_u32() { return static_cast<std::uint32_t>(pros::micros()); }

// Ein Steuer-Task fuer alle Phasen. Die Callbacks setzen nur g_phase, damit beim Uebergang
// Autonomie -> Fahrer kein Regler neu startet. Raster in millis() mit task_delay_until (kein Drift),
// Jitter aus micros()-Abstaenden. Nach einem Ueberzug wird das Raster uebersprungen statt nachgeholt.
void control_task(void*) {
    static frostbyte::ControlLoop loop(control_period_ms, pros::millis());
    frostbyte::Phase last_phase = g_phase.get();
    std::uint32_t phase_start_ms = pros::millis();
    std::uint32_t cycle_index = 0;  // Takte seit Start, bestimmt die Logzeile alle 20 ms
    std::uint32_t last_work_us = 0;
    bool phase_changed = false;
    while (true) {
        const std::uint32_t begin_us = micros_u32();
        loop.begin(begin_us);
        const frostbyte::Phase phase = g_phase.get();
        if (phase != last_phase) {
            phase_start_ms = pros::millis();
            phase_changed = true;
            // Maxima je Phase: Ausreisser beim Start (initialize, LCD) sollen den Fahrbetrieb nicht ueberdecken.
            loop.reset_stats();
            last_phase = phase;
        }
        // TODO: Sensoren lesen, Odometrie, Logik, Regler, Motoren schreiben (je nach phase).
        // TODO(RuleGuard): alle Motorausgaben durch g_rule_guard.filter() (M-07), mit GuardInput.disabled = (Phase == Disabled).
        if (phase != frostbyte::Phase::Disabled && frostbyte::LogCadence::due(cycle_index)) {
            // Nur kopieren (ADR-047): kein Schreiben, kein Blockieren. cpu_ms ist die Rechenzeit des Vortakts.
            frostbyte::LogRecord rec;
            rec.t_s = (pros::millis() - phase_start_ms) / 1000.0;
            rec.cpu_ms = last_work_us / 1000.0;
            rec.new_run = phase_changed;  // jeder Lauf eine eigene Datei, t_s beginnt bei 0 (ADR-047)
            if (phase_changed)
                frostbyte::LogRecord::set_text(
                    rec.event, phase == frostbyte::Phase::Autonomous ? "PHASE:AUTONOMOUS" : "PHASE:DRIVER");
            if (g_log.record(rec))
                phase_changed = false;
        }
        ++cycle_index;
        loop.end(pros::millis(), micros_u32());
        last_work_us = micros_u32() - begin_us;
        g_cycle_stats.publish(loop.stats());
        std::uint32_t base_ms = frostbyte::delay_until_base_ms(loop.wake_ms(), control_period_ms);
        pros::c::task_delay_until(&base_ms, control_period_ms);
    }
}

// Schreibt die Logzeilen auf die SD-Karte (F-05). Eigener Task mit niedriger Prioritaet: Karte und Dateisystem duerfen
// stocken, ohne den Steuer-Task zu beruehren.
void logger_task(void*) {
    std::uint32_t now_ms = pros::millis();
    while (true) {
        g_log.drain(g_log_sink);
        pros::c::task_delay_until(&now_ms, logger_period_ms);
    }
}

// Zeigt die Takt-Statistik der aktuellen Phase auf dem LCD (Ablesen am Roboter; der Logger aus F-05 schreibt sie noch nicht ins Log).
// Eigener Task mit niedriger Prioritaet: die Ausgabe bleibt aus dem Steuer-Task heraus. Liest nur, setzt nie zurueck.
void display_task(void*) {
    std::uint32_t now_ms = pros::millis();
    while (true) {
        if constexpr (config::link_enabled && config::link_selftest) {
            // Hardware-Pruefung L-02: Link-Stand statt Takt-Statistik (Zeilen 2/3).
            const frostbyte::LinkSnapshot l = frostbyte::link_shared().snapshot(pros::millis());
            if (l.valid) {
                pros::lcd::print(2, "link %s offen %u ack %lu exp %lu", l.connected ? "UP" : "weg",
                                 static_cast<unsigned>(l.pending), static_cast<unsigned long>(l.acked),
                                 static_cast<unsigned long>(l.expired));
                pros::lcd::print(3, "ping_test %s in %lu schlecht %lu", l.selftest_seen ? "gesehen" : "nein",
                                 static_cast<unsigned long>(l.frames_in), static_cast<unsigned long>(l.frames_bad));
            }
        } else {
            const frostbyte::CycleStatsView v = g_cycle_stats.load();
            pros::lcd::print(2, "jitter max %lu us", static_cast<unsigned long>(v.jitter_max_us));
            pros::lcd::print(3, "cpu max %lu us  ueberzuege %lu", static_cast<unsigned long>(v.work_max_us),
                             static_cast<unsigned long>(v.overruns));
        }
        pros::c::task_delay_until(&now_ms, display_period_ms);
    }
}

// Routinenmenue auf den LCD-Zeilen 4..7 (Zeile 1..3 gehoeren Name und Takt-Anzeige). Die drei Tasten des Brain-Displays
// waehlen (links/rechts) und speichern (Mitte). Waehrend der Autonomie gesperrt; sonst (auch ohne Field Control) bedienbar.
// TODO(HW): am Brain pruefen, ob read_buttons() links/Mitte/rechts so liefert (LCD_BTN_LEFT=4, CENTER=2, RIGHT=1).
void menu_task(void*) {
    std::uint32_t now_ms = pros::millis();
    while (true) {
        const std::uint8_t b = pros::lcd::read_buttons();
        const frostbyte::Buttons buttons{(b & LCD_BTN_LEFT) != 0, (b & LCD_BTN_CENTER) != 0, (b & LCD_BTN_RIGHT) != 0};
        g_routine_screen.update(g_routine_store, buttons, g_phase.get() != frostbyte::Phase::Autonomous);
        frostbyte::ScreenText text;
        g_routine_screen.render(text);
        for (int i = 0; i < frostbyte::ScreenText::rows; ++i)
            pros::lcd::print(4 + i, "%s", text.line[i]);  // leerer Text loescht eine alte Zeile
        pros::c::task_delay_until(&now_ms, menu_period_ms);
    }
}

}  // namespace

void initialize() {
    pros::lcd::initialize();
    pros::lcd::set_text(1, config::robot_name);
    {
        // Version, Commit, dirty/Datum auf Zeile 0 (Startbildschirm); einmalig, nicht im Steuer-Task.
        char text[frostbyte::BuildInfoText::cols + 1];
        frostbyte::format_build_info(frostbyte::BuildInfo{FB_VERSION, FB_COMMIT, FB_DIRTY != 0, __DATE__}, text,
                                     sizeof text);
        pros::lcd::set_text(0, text);
    }
    if constexpr (config::tuning_enabled && !config::motion_uses_lemlib_pid) {
        frostbyte::TuningSet tuned{config::lateral_pid, config::angular_pid};
        if (frostbyte::load_tuning(g_tuning_store, tuned) == frostbyte::LoadResult::loaded)
            frostbyte::apply_tuning(tuned, g_lat_pid, g_ang_pid);
    }
    g_routine_screen.init(g_routine_store);
    if constexpr (config::link_enabled)
        (void)frostbyte::link_shared();  // Mutex und Endpunkt hier anlegen, nicht erst im Steuer-Task
    if constexpr (config::port_check_enabled || config::link_test_enabled)
        return;  // nur Pruefprogramm (F-03, L-01): kein Steuer-Task, opcontrol() startet es
    // Log vor dem Steuer-Task oeffnen (ADR-047); ohne Karte laeuft der Roboter trotzdem, nur ohne Log (Warnung: F-06).
    const bool logging = g_log.open(g_log_sink);
    static pros::Task task(control_task, nullptr, TASK_PRIORITY_MAX - 1, TASK_STACK_DEPTH_DEFAULT, "control");
    if (logging)
        static pros::Task logger(logger_task, nullptr, TASK_PRIORITY_MIN + 1, TASK_STACK_DEPTH_DEFAULT, "logger");
    static pros::Task display(display_task, nullptr, TASK_PRIORITY_MIN + 1, TASK_STACK_DEPTH_DEFAULT, "display");
    static pros::Task menu(menu_task, nullptr, TASK_PRIORITY_MIN + 1, TASK_STACK_DEPTH_DEFAULT, "menu");
    if constexpr (config::
                      link_enabled)  // Funk zum Partner (L-02): eigener Task, der Steuer-Task fasst pros::Link nie an
        static pros::Task comms(frostbyte::comms_task, nullptr, TASK_PRIORITY_MIN + 1, TASK_STACK_DEPTH_DEFAULT,
                                "comms");
}

void disabled() { g_phase.set(frostbyte::Phase::Disabled); }

void competition_initialize() { g_phase.set(frostbyte::Phase::Disabled); }

void autonomous() { g_phase.set(frostbyte::Phase::Autonomous); }

void opcontrol() {
    if constexpr (config::port_check_enabled)
        frostbyte::port_check_run(config::port_expect, config::port_expect_count);
    if constexpr (config::link_test_enabled)
        frostbyte::link_test_run();  // kehrt nie zurueck
    g_phase.set(frostbyte::Phase::Driver);
    if constexpr (config::port_check_enabled)
        return;  // Pruefprogramm (F-03) verhaelt sich wie vorher: Controller-Anzeige gehoert dem Scan
    // Rückmeldung an den Fahrer (C-04) hier, nicht im Steuer-Task: Controllerbefehle blockieren und sind auf 50 ms getaktet.
    static frostbyte::hal::ProsControllerOutput controller;
    frostbyte::ControllerFeedback feedback(controller, driver_run_s);
    const std::uint32_t start_ms = pros::millis();
    std::uint32_t now_ms = start_ms;
    while (true) {  // PROS beendet opcontrol(), sobald der Roboter deaktiviert wird
        feedback.update((now_ms - start_ms) / 1000.0, config::robot_name);
        pros::c::task_delay_until(&now_ms, feedback_period_ms);
    }
}
