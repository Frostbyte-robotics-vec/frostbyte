#pragma once

#include <cmath>

#include "core/units.hpp"
#include "estimation/drive_odometry.hpp"
#include "estimation/odometry.hpp"
#include "estimation/pose.hpp"

// Odometrie Stacker mit Rückfall (D-08, ADR-018): Tracking-Räder plus IMU im Normalbetrieb, bei Ausfall eines
// Tracking-Rads (oder des IMU) endgültig DriveOdometry (Antriebsencoder). Die aktive Pose gehört dieser Klasse; die
// jeweils andere Odometrie wird jeden Takt auf sie gesetzt, deshalb gibt es beim Umschalten keinen Posensprung.
// Erst prüfen, dann integrieren: der Takt, der den Ausfall zeigt, und Verdachtstakte rechnen mit den Antriebsencodern.
namespace frostbyte {

struct FallbackOdometryParams {
    TrackingGeometry geometry;
    DriveOdometryParams drive;
    double jump_in;  // Abweichung eines Tracking-Rads zum Erwartungswert in einem Takt, ab der es als ausgefallen gilt
    double still_wheel_in;  // Radweg je Takt darunter gilt als "steht" ...
    double still_drive_in;  // ... wenn der Erwartungswert (aus Antriebsencodern und Drehung) darüber liegt
    int still_cycles;       // so viele Takte in Folge = Rad eingefroren
};

enum class TrackedFallbackReason {
    none,
    parallel_invalid,
    parallel_jump,
    parallel_stall,
    perpendicular_invalid,
    perpendicular_jump,
    perpendicular_stall,
    imu
};

class FallbackOdometry {
public:
    explicit FallbackOdometry(const FallbackOdometryParams& p) : p_(p), tracking_(p.geometry), drive_(p.drive) {}

    // Setzt Pose und Bezugswerte (alle Summen und das IMU-Heading als Rohwerte dieses Moments) und hebt einen früheren
    // Rückfall auf (neues Match).
    void reset(const Pose& pose, double left_total_in, double right_total_in, double parallel_total_in,
               double perpendicular_total_in, double imu_heading_deg) {
        pose_ = pose;
        pose_.heading_deg = wrap_deg(pose.heading_deg);
        tracking_.reset(pose_, parallel_total_in, perpendicular_total_in, imu_heading_deg);
        drive_.reset(pose_, left_total_in, right_total_in, imu_heading_deg);
        last_left_in_ = left_total_in;
        last_right_in_ = right_total_in;
        last_par_in_ = parallel_total_in;
        last_perp_in_ = perpendicular_total_in;
        reason_ = TrackedFallbackReason::none;
        par_still_ = perp_still_ = 0;
    }

    // Einmal je Steuertakt. *_valid = false, wenn die HAL für diesen Takt keine gültige Messung hat.
    void update(double left_total_in, double right_total_in, double parallel_total_in, bool parallel_valid,
                double perpendicular_total_in, bool perpendicular_valid, double imu_heading_deg,
                bool imu_valid = true) {
        const Pose before = pose_;
        // Die Schattenodometrie rechnet von der aktiven Pose aus; ihre Drehung dieses Takts (IMU oder Encoder,
        // je nach Zustand) dient als Erwartungswert für die Räder.
        drive_.update(left_total_in, right_total_in, imu_heading_deg, imu_valid);
        const Pose drive_pose = drive_.pose();

        bool use_drive = degraded();
        if (!degraded()) {
            const double forward_in = ((left_total_in - last_left_in_) + (right_total_in - last_right_in_)) / 2.0;
            const double dtheta = deg_to_rad(wrap_deg(drive_pose.heading_deg - before.heading_deg));
            check(parallel_total_in, parallel_valid, last_par_in_, forward_in - dtheta * p_.geometry.parallel_y_in,
                  par_still_, TrackedFallbackReason::parallel_invalid, TrackedFallbackReason::parallel_jump,
                  TrackedFallbackReason::parallel_stall);
            check(perpendicular_total_in, perpendicular_valid, last_perp_in_, dtheta * p_.geometry.perpendicular_x_in,
                  perp_still_, TrackedFallbackReason::perpendicular_invalid, TrackedFallbackReason::perpendicular_jump,
                  TrackedFallbackReason::perpendicular_stall);
            if (!degraded() && drive_.degraded())
                reason_ = TrackedFallbackReason::imu;
            use_drive = degraded() || par_still_ > 0 || perp_still_ > 0 || drive_.imu_suspect();
        }

        if (use_drive) {
            pose_ = drive_pose;
            // Verdachtstakt (noch nicht bestätigt): die Tracking-Seite mit neuen Bezugswerten wieder anschließen.
            if (!degraded() && finite(parallel_total_in, parallel_valid) &&
                finite(perpendicular_total_in, perpendicular_valid))
                tracking_.reset(pose_, parallel_total_in, perpendicular_total_in, imu_heading_deg);
        } else {
            tracking_.update(parallel_total_in, perpendicular_total_in, imu_heading_deg);
            pose_ = tracking_.pose();
            drive_.set_pose(pose_);
        }

        last_left_in_ = left_total_in;
        last_right_in_ = right_total_in;
        if (finite(parallel_total_in, parallel_valid))
            last_par_in_ = parallel_total_in;
        if (finite(perpendicular_total_in, perpendicular_valid))
            last_perp_in_ = perpendicular_total_in;
    }

    const Pose& pose() const { return pose_; }
    bool degraded() const { return reason_ != TrackedFallbackReason::none; }
    TrackedFallbackReason fallback_reason() const { return reason_; }

private:
    static bool finite(double total_in, bool valid) { return valid && std::isfinite(total_in); }

    // Prüft ein Rad gegen seinen Erwartungswert. Ungültig und Sprung schalten sofort um, Stillstand nach still_cycles.
    void check(double total_in, bool valid, double last_in, double expected_in, int& still,
               TrackedFallbackReason invalid, TrackedFallbackReason jump, TrackedFallbackReason stall) {
        if (degraded())
            return;  // der erste Befund bleibt als Grund stehen
        if (!finite(total_in, valid)) {
            reason_ = invalid;
            return;
        }
        const double measured_in = total_in - last_in;
        if (std::fabs(measured_in - expected_in) > p_.jump_in) {
            reason_ = jump;
        } else if (std::fabs(expected_in) > p_.still_drive_in && std::fabs(measured_in) < p_.still_wheel_in) {
            if (++still >= p_.still_cycles)
                reason_ = stall;
        } else {
            still = 0;
        }
    }

    FallbackOdometryParams p_;
    TrackingOdometry tracking_;
    DriveOdometry drive_;
    Pose pose_;
    double last_left_in_ = 0.0;
    double last_right_in_ = 0.0;
    double last_par_in_ = 0.0;
    double last_perp_in_ = 0.0;
    TrackedFallbackReason reason_ = TrackedFallbackReason::none;
    int par_still_ = 0;
    int perp_still_ = 0;
};

}  // namespace frostbyte
