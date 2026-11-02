#pragma once

#include <functional>
#include <utility>

#include "control/motion_controller.hpp"
#include "sim/drive.hpp"

// Sim-Adapter (D-05, ADR-017): hängt einen MotionController als RobotSetup::closed_loop in run_match ein.
// Er bekommt je Steuertakt die geschätzte Pose (Sensormodelle + Odometrie der Simulation), nicht die Wahrheit.
// Der Controller muss den Match überleben (Referenz).
namespace frostbyte::sim {

inline std::function<std::pair<double, double>(int, const Pose&)> motion_loop(::frostbyte::MotionController& mc) {
    return [&mc](int t_ms, const Pose& est) {
        const ::frostbyte::MotionOutput o = mc.update(t_ms / 1000.0, ::frostbyte::Pose{est.x_in, est.y_in, est.h_deg});
        return std::make_pair(o.left_v, o.right_v);
    };
}

}  // namespace frostbyte::sim
