#pragma once

#include <cmath>

#include "core/units.hpp"
#include "estimation/pose.hpp"

// Odometrie aus den Antriebsencodern plus IMU (D-03, ADR-015): Runner im Normalbetrieb, Stacker als Rückfall.
// Position aus dem mittleren Weg beider Antriebsseiten, Heading aus dem IMU. Fällt das IMU aus (ungültig, Sprung,
// eingefroren), wechselt die Odometrie endgültig auf die Drehung aus den Encodern; die Pose bleibt dabei stehen,
// weitergerechnet werden nur Zuwächse. Das IMU-Vorzeichen und die Einheiten müssen schon stimmen, bevor die Werte
// hier ankommen (HAL/config, TODO(HW)).
namespace frostbyte {

struct DriveOdometryParams {
    double track_width_in;     // wirksame Spurbreite (Mitte linke bis Mitte rechte Spur), am Roboter einzumessen
    double jump_deg;           // Abweichung IMU- zu Encoder-Drehung in einem Takt, ab der das IMU als ausgefallen gilt
    double still_imu_deg;      // IMU-Drehung je Takt darunter gilt als "steht"
    double still_encoder_deg;  // Encoder-Drehung je Takt darüber gilt als "dreht"
    int still_cycles;          // so viele Takte in Folge "IMU steht, Encoder drehen" = IMU eingefroren
};

enum class FallbackReason { none, imu_invalid, imu_jump, imu_frozen };

class DriveOdometry {
public:
    explicit DriveOdometry(const DriveOdometryParams& p) : p_(p) {}

    // Setzt Pose und Bezugswerte und hebt eine frühere Umschaltung auf (neues Match). Die Wegsummen und das
    // IMU-Heading sind die Rohwerte dieses Moments.
    void reset(const Pose& pose, double left_total_in, double right_total_in, double imu_heading_deg) {
        pose_ = pose;
        pose_.heading_deg = wrap_deg(pose.heading_deg);
        last_left_in_ = left_total_in;
        last_right_in_ = right_total_in;
        last_imu_deg_ = imu_heading_deg;
        reason_ = FallbackReason::none;
        still_count_ = 0;
    }

    // Einmal je Steuertakt. imu_valid = false, wenn die HAL für diesen Takt keine gültige IMU-Messung hat.
    void update(double left_total_in, double right_total_in, double imu_heading_deg, bool imu_valid = true) {
        const double dl = left_total_in - last_left_in_;
        const double dr = right_total_in - last_right_in_;
        const double enc_dtheta_deg = rad_to_deg((dr - dl) / p_.track_width_in);
        const bool imu_ok = imu_valid && std::isfinite(imu_heading_deg);

        // Zuerst prüfen, dann integrieren: der Takt, der den Ausfall zeigt, darf nicht mehr in die Pose eingehen.
        bool use_encoder = degraded();
        if (!degraded()) {
            if (!imu_ok) {
                reason_ = FallbackReason::imu_invalid;
                use_encoder = true;
            } else {
                const double imu_dtheta_deg = wrap_deg(imu_heading_deg - last_imu_deg_);
                if (std::fabs(wrap_deg(imu_dtheta_deg - enc_dtheta_deg)) > p_.jump_deg) {
                    reason_ = FallbackReason::imu_jump;
                    use_encoder = true;
                } else if (std::fabs(imu_dtheta_deg) < p_.still_imu_deg &&
                           std::fabs(enc_dtheta_deg) > p_.still_encoder_deg) {
                    // Verdacht: schon jetzt mit der Encoder-Drehung rechnen, sonst läuft das Heading in der Wartezeit weg.
                    use_encoder = true;
                    if (++still_count_ >= p_.still_cycles)
                        reason_ = FallbackReason::imu_frozen;
                } else {
                    still_count_ = 0;
                }
            }
        }

        const double dtheta_deg = use_encoder ? enc_dtheta_deg : wrap_deg(imu_heading_deg - last_imu_deg_);
        advance_pose(pose_, (dl + dr) / 2.0, 0.0, dtheta_deg);

        last_left_in_ = left_total_in;
        last_right_in_ = right_total_in;
        if (imu_ok)
            last_imu_deg_ = imu_heading_deg;
    }

    // Überschreibt nur die Pose; Bezugswerte und Ausfallzustand bleiben (anders als reset()). Für die Stacker-Odometrie
    // (D-08): die Schattenodometrie wird jeden Takt auf die aktive Pose gesetzt, behält aber ihre IMU-Vorgeschichte.
    void set_pose(const Pose& pose) {
        pose_ = pose;
        pose_.heading_deg = wrap_deg(pose.heading_deg);
    }

    const Pose& pose() const { return pose_; }
    // Verdacht auf eingefrorenes IMU, noch nicht bestätigt: dieser Takt rechnet schon mit der Encoder-Drehung.
    bool imu_suspect() const { return !degraded() && still_count_ > 0; }
    bool degraded() const { return reason_ != FallbackReason::none; }
    FallbackReason fallback_reason() const { return reason_; }

private:
    DriveOdometryParams p_;
    Pose pose_;
    double last_left_in_ = 0.0;
    double last_right_in_ = 0.0;
    double last_imu_deg_ = 0.0;
    FallbackReason reason_ = FallbackReason::none;
    int still_count_ = 0;
};

}  // namespace frostbyte
