#pragma once

namespace config {
inline constexpr const char* robot_name = "stacker";
inline constexpr double footprint_in = 24.0;  // SG2
// TODO(HW): Lage der Tracking-Raeder (Versatz vom Drehmittelpunkt, Zoll) fuer estimation/odometry.hpp:
//   parallel_y_in (Parallelrad, links positiv), perpendicular_x_in (Querrad, vorn positiv). Ebenso Rad-Durchmesser,
//   Ports, Drehrichtung und das IMU-Vorzeichen (Feld: gegen den Uhrzeigersinn positiv).
// TODO(HW): Ports, Drehrichtungen, Uebersetzungen, Radgroessen folgen mit dem Antriebs-Ticket
}  // namespace config
