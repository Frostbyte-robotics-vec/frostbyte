#pragma once

#include "match/driver_map.hpp"

namespace config::stacker {
inline constexpr const char* robot_name = "stacker";
inline constexpr double footprint_in = 24.0;  // SG2
// TODO(HW): Lage der Tracking-Raeder (Versatz vom Drehmittelpunkt, Zoll) fuer estimation/odometry.hpp:
//   parallel_y_in (Parallelrad, links positiv), perpendicular_x_in (Querrad, vorn positiv). Ebenso Rad-Durchmesser,
//   Ports, Drehrichtung und das IMU-Vorzeichen (Feld: gegen den Uhrzeigersinn positiv).
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
}  // namespace config::stacker
