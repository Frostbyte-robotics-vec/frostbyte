#pragma once

#include "control/motion_controller.hpp"
#include "match/driver_map.hpp"
#include "match/route_player.hpp"
#include "tasks/cycle_controller.hpp"
#include "util/port_check.hpp"
#include "util/rule_guard.hpp"

namespace config::stacker {
inline constexpr const char* robot_name = "stacker";
inline constexpr double footprint_in = 24.0;  // SG2
// TODO(HW): Lage der Tracking-Raeder (Versatz vom Drehmittelpunkt, Zoll) fuer estimation/odometry.hpp:
//   parallel_y_in (Parallelrad, links positiv), perpendicular_x_in (Querrad, vorn positiv). Ebenso Rad-Durchmesser,
//   Ports, Drehrichtung und das IMU-Vorzeichen (Feld: gegen den Uhrzeigersinn positiv).
// TODO(HW): GPS-Fusion (estimation/gps_fusion.hpp, ADR-016): max_error_in, max_speed_in_s, gate_in, position_gain, heading_gain
//   und Einbaulage/Versatz des GPS-Sensors am Roboter; Werte aus echten GPS-Logs ableiten.
// TODO(HW): Ports, Drehrichtungen, Uebersetzungen, Radgroessen folgen mit dem Antriebs-Ticket
// TODO(HW): Rueckfall auf Antriebsencoder (estimation/drive_odometry.hpp, ADR-015): track_width_in und Erkennungsschwellen,
//   sobald die Umschaltung bei Ausfall eines Tracking-Rads verdrahtet wird (Folgeticket D-08).

// Fahrer-Belegung (C-03, ADR-014). Grundbelegung identisch auf beiden Robotern; Fahrer bestaetigt sie am Roboter.
inline constexpr frostbyte::Binding bindings[] = {
    {frostbyte::Action::grab, frostbyte::Button::r1},         {frostbyte::Action::place, frostbyte::Button::r2},
    {frostbyte::Action::rotate_cup, frostbyte::Button::l1},   {frostbyte::Action::slow, frostbyte::Button::l2},
    {frostbyte::Action::hold, frostbyte::Button::b},          {frostbyte::Action::toggle_place, frostbyte::Button::a},
    {frostbyte::Action::assist_toggle, frostbyte::Button::x}, {frostbyte::Action::endgame, frostbyte::Button::y},
};
inline constexpr int bindings_count = sizeof(bindings) / sizeof(bindings[0]);

// Fahrerprofile: Tuning-Startwerte, pro Fahrer am Roboter einstellen (TODO(HW)).
inline constexpr frostbyte::DriverProfile profiles[] = {
    {"Standard", {.deadband = 0.05, .exponent = 2.0, .max_speed = 1.0}},
    {"Vorsichtig", {.deadband = 0.08, .exponent = 3.0, .max_speed = 0.7}},
};
inline constexpr int profile_count = sizeof(profiles) / sizeof(profiles[0]);
// TODO(HW): Lift (subsystems/lift.hpp, ADR-019): Sensor (Rotation oder Motorencoder), Port, Drehrichtung, Homing-Richtung
//   (home_v-Vorzeichen), die 5 Kalibrierpunkte (Sensorwinkel relativ zu Home -> gemessene Hoehe in Zoll), min_in/max_in,
//   kp, kg, Spannungsgrenzen, Timeouts. Nichts davon raten; Messanleitung in tasks/M-01.md.

// Bewegung (D-05, ADR-017): Startwerte, getunt wird am Testchassis in D-07 (TODO(HW)). Toleranzen und Blocked-Fenster
// sind Software-Entscheidungen; Verstaerkungen und Spurbreite haengen von der Hardware ab.
// TODO(HW): Spurbreite messen (Mitte Rad links bis Mitte Rad rechts); 14.0 ist die Schaetzung aus sim_params.json
inline constexpr frostbyte::MotionConfig motion{
    .max_volts = 12.0,
    .track_width_in = 14.0,
    .turn_tol_deg = 2.0,
    .move_tol_in = 1.0,
    .settle_s = 0.1,
    .near_in = 4.0,
    .lookahead_in = 10.0,
    .follow_max_volts = 8.0,
    .blocked_window_s = 0.5,
    .blocked_min_effort_v = 2.0,
    .blocked_min_motion_in = 0.5,
    .blocked_min_motion_deg = 2.0,
};
// TODO(HW): PID-Verstaerkungen am Roboter tunen (D-07). lateral: V je Zoll, angular: V je Grad.
inline constexpr frostbyte::PidGains lateral_pid{
    .kp = 1.0, .ki = 0.0, .kd = 0.08, .i_zone = 0.0, .i_max_v = 2.0, .out_max_v = 12.0};
inline constexpr frostbyte::PidGains angular_pid{
    .kp = 0.15, .ki = 0.5, .kd = 0.01, .i_zone = 10.0, .i_max_v = 2.0, .out_max_v = 12.0};
// false: eigener Pid (wie in der Simulation). true: lemlib::PID ueber src/lemlib_regulator.hpp, nur zum Vergleich in D-07.
inline constexpr bool motion_uses_lemlib_pid = false;
// Tuning-Modus (D-06, ADR-020): PID-Werte per Controller aendern und auf der SD-Karte speichern. Nur am Testchassis
// einschalten (Tasten A/B/X und das Steuerkreuz gehoeren dann dem Tuning, nicht dem Fahrer); im Match immer false.
// Gilt nur mit dem eigenen Pid (motion_uses_lemlib_pid = false). Uebernahme ins Repo: Werte hier eintragen (D-07).
inline constexpr bool tuning_enabled = false;

// RuleGuard (M-07, ADR-027). TODO(HW): lift_top_offset_in = Hoehe der hoechsten Roboterstelle ueber dem Boden minus gemessene
// Lifthoehe (bei jeder Lifthoehe konstant, sonst Kurve noetig); Messung in M-09. NaN (Standard) sperrt den Lift
// nach oben immer. Geraten wird nichts.
// TODO(HW) (M-09, ADR-027 Nachtrag): lift_footprint_ceiling_in (Lifthoehe, ab der der Footprint 24" ueberschreitet; +inf wenn nie),
//   arm_min_deg/arm_max_deg (Armbereich mit Footprint <= 24" und Hoehe <= 50", Worst Case ueber die Lifthoehe). NaN (Standard) sperrt
//   Lift nach oben und Arm in beide Richtungen. Messung in M-11.
inline constexpr frostbyte::RuleGuardConfig rule_guard{};

// Zyklus (C-01, ADR-028). Die Zustands-Timeouts sind Startwerte (Software-Entscheidung), am Roboter pruefen.
// TODO(HW): grip_offset_in = Hoehe des Greifers ueber der gemessenen Lifthoehe (und retreat_lift_in = Lifthoehe beim Rueckzug).
// NaN (Standard) lehnt jeden Zyklus ab bzw. haelt beim Rueckzug die Hoehe. Geraten wird nichts.
inline constexpr frostbyte::CycleConfig cycle{};

// Routen (A-03, ADR-029): Standard-Timeout je Befehlsart, wenn die Route keins nennt. Startwerte (Software-Entscheidung), am
// Roboter und in der Simulation (A-05) pruefen.
inline constexpr frostbyte::RoutePlayerConfig route_player{.goto_s = 8.0,
                                                           .turn_s = 4.0,
                                                           .intake_s = 4.0,
                                                           .cycle_s = 12.0,
                                                           .toggle_s = 4.0,
                                                           .wait_for_s = 10.0,
                                                           .other_s = 1.0};

// port_check (F-03): Erwartung je Port (Art, Name, Drehrichtung des Encoders bei +Spannung ohne Port-Umkehr). Leer, bis die
// Ports feststehen (F-07). Mit port_check_enabled = true startet der Roboter nur das Pruefprogramm (src/port_check.hpp); im Match false.
// TODO(HW): Eintraege aus docs/ports.md (F-07) uebernehmen; Ports und Richtungen werden nicht geraten.
inline constexpr const frostbyte::PortExpect* port_expect = nullptr;
inline constexpr int port_expect_count = 0;
inline constexpr bool port_check_enabled = false;

}  // namespace config::stacker
