#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "control/pid.hpp"
#include "core/state_machine.hpp"
#include "estimation/odometry.hpp"

// Schicht 4: Bewegung zu Pose (D-05, ADR-017). turn_to, move_to, follow (Pure Pursuit) für einen Differentialantrieb.
// Wie ein Subsystem nur request(), state(), update(); update() bekommt Zeit und Pose und gibt Spannung links/rechts zurück
// (die aufrufende Schicht gibt sie an die Motoren bzw. das Sim-Fahrmodell weiter). Kein dynamischer Speicher.
// Konventionen wie im Feldplan: Zoll, Grad, Sekunden; Heading gegen den Uhrzeigersinn positiv, h in (-180, 180].
// Jeder Befehl endet in genau einem Endzustand: done (Ziel erreicht und gehalten), timeout (Zeit um) oder
// blocked (Spannung liegt an, aber der Roboter bewegt sich ein ganzes Fenster lang nicht). In allen Endzuständen 0 V.
namespace frostbyte {

struct MotionConfig {
    double max_volts = 12.0;
    double track_width_in = 12.0;         // follow: Spurbreite zur Umrechnung der Krümmung in links/rechts
    double turn_tol_deg = 2.0;            // turn_to fertig innerhalb dieser Abweichung ...
    double move_tol_in = 1.0;             // move_to/follow fertig innerhalb dieses Abstands ...
    double settle_s = 0.1;                // ... wenn sie so lange gehalten wird
    double near_in = 4.0;                 // move_to: näher als das wird nicht mehr nachgelenkt (kein Kreiseln am Ziel)
    double lookahead_in = 10.0;           // follow: Vorausschau-Radius
    double follow_max_volts = 8.0;        // follow: Grenze für die Fahrspannung, damit für die Lenkung Luft bleibt
    double blocked_window_s = 0.5;        // blocked: Länge des Prüffensters
    double blocked_min_effort_v = 2.0;    // blocked nur, wenn die Spannung im ganzen Fenster mindestens so hoch war
    double blocked_min_motion_in = 0.5;   // und der Roboter weniger als das gefahren ist
    double blocked_min_motion_deg = 2.0;  // und sich weniger als das gedreht hat
};

struct PathPoint {
    double x_in = 0.0;
    double y_in = 0.0;
};

// Pfad mit fester Kapazität. Ein Punkt zu viel oder ein ungültiger Punkt wird abgelehnt und macht den Pfad
// ungültig, damit nie ein abgeschnittener Pfad gefahren wird.
class Path {
public:
    static constexpr int capacity = 32;

    bool add(double x_in, double y_in) {
        if (!std::isfinite(x_in) || !std::isfinite(y_in)) {
            invalid_ = true;
            return false;
        }
        if (count_ >= capacity) {
            overflowed_ = true;
            return false;
        }
        pts_[count_++] = PathPoint{x_in, y_in};
        return true;
    }

    int size() const { return count_; }
    bool overflowed() const { return overflowed_; }
    bool valid() const { return count_ > 0 && !overflowed_ && !invalid_; }
    const PathPoint& operator[](int i) const { return pts_[i]; }

private:
    std::array<PathPoint, capacity> pts_{};
    int count_ = 0;
    bool overflowed_ = false;
    bool invalid_ = false;
};

enum class MotionKind { turn_to, move_to, follow };

struct MotionCommand {
    MotionKind kind = MotionKind::turn_to;
    double heading_deg = 0.0;  // turn_to
    double x_in = 0.0;         // move_to
    double y_in = 0.0;
    bool reverse = false;    // move_to: mit dem Heck voran
    Path path;               // follow
    double timeout_s = 0.0;  // Pflicht: endlich und > 0

    static MotionCommand turn_to(double heading_deg, double timeout_s) {
        MotionCommand c;
        c.kind = MotionKind::turn_to;
        c.heading_deg = heading_deg;
        c.timeout_s = timeout_s;
        return c;
    }
    static MotionCommand move_to(double x_in, double y_in, double timeout_s, bool reverse = false) {
        MotionCommand c;
        c.kind = MotionKind::move_to;
        c.x_in = x_in;
        c.y_in = y_in;
        c.reverse = reverse;
        c.timeout_s = timeout_s;
        return c;
    }
    static MotionCommand follow(const Path& path, double timeout_s) {
        MotionCommand c;
        c.kind = MotionKind::follow;
        c.path = path;
        c.timeout_s = timeout_s;
        return c;
    }
};

enum class MotionState { idle, running, done, timeout, blocked };

struct MotionOutput {
    double left_v = 0.0;
    double right_v = 0.0;
};

class MotionController {
public:
    // lateral regelt den Abstand (Zoll), angular den Winkel (Grad). Die Regler gehören dem Aufrufer.
    MotionController(const MotionConfig& cfg, Regulator& lateral, Regulator& angular)
        : cfg_(cfg), lat_(lateral), ang_(angular), sm_(MotionState::idle, 0.0) {}

    // Ersetzt den laufenden Befehl sofort. Ungültiger Befehl (Timeout nicht endlich oder <= 0, Ziel nicht endlich,
    // Pfad leer/übervoll/ungültig): false, und der Controller steht (idle, 0 V).
    bool request(const MotionCommand& cmd, double now_s) {
        if (!valid(cmd)) {
            sm_.enter(MotionState::idle, now_s);
            return false;
        }
        cmd_ = cmd;
        lat_.reset();
        ang_.reset();
        seg_ = 0;
        approached_ = false;
        has_last_ = false;
        settling_ = false;
        window_open_ = false;
        sm_.enter(MotionState::running, now_s, cmd.timeout_s, MotionState::timeout);
        return true;
    }

    MotionState state() const { return sm_.state(); }

    // Einmal je Steuertakt. Ungültige Pose (NaN/inf): 0 V in diesem Takt, Zeit und Timeout laufen weiter.
    MotionOutput update(double now_s, const Pose& pose) {
        sm_.update(now_s);
        if (sm_.state() != MotionState::running)
            return MotionOutput{};
        if (!std::isfinite(pose.x_in) || !std::isfinite(pose.y_in) || !std::isfinite(pose.heading_deg))
            return MotionOutput{};
        const double dt_s = has_last_ ? now_s - last_s_ : 0.0;
        last_s_ = now_s;
        has_last_ = true;

        MotionOutput out;
        bool in_tol = false;
        switch (cmd_.kind) {
            case MotionKind::turn_to: in_tol = turn(pose, dt_s, out); break;
            case MotionKind::move_to: in_tol = move(pose, cmd_.x_in, cmd_.y_in, cmd_.reverse, dt_s, out); break;
            case MotionKind::follow: in_tol = follow(pose, dt_s, out); break;
        }

        if (in_tol) {
            if (!settling_) {
                settling_ = true;
                settle_since_s_ = now_s;
            }
            if (now_s - settle_since_s_ >= cfg_.settle_s) {
                sm_.enter(MotionState::done, now_s);
                return MotionOutput{};
            }
        } else {
            settling_ = false;
        }

        if (stuck(pose, now_s, std::max(std::fabs(out.left_v), std::fabs(out.right_v)))) {
            sm_.enter(MotionState::blocked, now_s);
            return MotionOutput{};
        }
        return out;
    }

private:
    static bool valid(const MotionCommand& c) {
        if (!std::isfinite(c.timeout_s) || c.timeout_s <= 0.0)
            return false;
        switch (c.kind) {
            case MotionKind::turn_to: return std::isfinite(c.heading_deg);
            case MotionKind::move_to: return std::isfinite(c.x_in) && std::isfinite(c.y_in);
            case MotionKind::follow: return c.path.valid();
        }
        return false;
    }

    // Fahranteil lin und Drehanteil a (gegen den Uhrzeigersinn positiv) mischen; die Drehung hat Vorrang.
    void mix(double lin, double a, double max_lin, MotionOutput& out) const {
        a = std::clamp(a, -cfg_.max_volts, cfg_.max_volts);
        const double room = std::min(max_lin, cfg_.max_volts - std::fabs(a));
        lin = std::clamp(lin, -room, room);
        out.left_v = lin - a;
        out.right_v = lin + a;
    }

    bool turn(const Pose& p, double dt_s, MotionOutput& out) {
        const double err = wrap_deg(cmd_.heading_deg - p.heading_deg);
        mix(0.0, ang_.update(err, dt_s), cfg_.max_volts, out);
        return std::fabs(err) <= cfg_.turn_tol_deg;
    }

    bool move(const Pose& p, double x_in, double y_in, bool reverse, double dt_s, MotionOutput& out) {
        const double dx = x_in - p.x_in;
        const double dy = y_in - p.y_in;
        const double d = std::hypot(dx, dy);
        const double facing_deg = reverse ? p.heading_deg + 180.0 : p.heading_deg;
        const double he = wrap_deg(rad_to_deg(std::atan2(dy, dx)) - facing_deg);
        // Abstand längs der Blickrichtung: nach dem Überfahren negativ, also fährt der Roboter zurück.
        double lin = lat_.update(d * std::cos(deg_to_rad(he)), dt_s);
        if (reverse)
            lin = -lin;
        // War der Roboter schon nah am Ziel und ist darüber hinaus, wendet er nicht, sondern richtet das Heck
        // (bzw. die Front) zum Ziel und fährt zurück: Lenkfehler auf -90..90 falten.
        if (d <= cfg_.near_in)
            approached_ = true;
        double steer = he;
        if (approached_ && std::fabs(he) > 90.0)
            steer = wrap_deg(he + 180.0);
        double a = 0.0;
        if (d > cfg_.near_in)
            a = ang_.update(steer, dt_s);
        else
            ang_.reset();
        mix(lin, a, cfg_.max_volts, out);
        return d <= cfg_.move_tol_in;
    }

    // Pure Pursuit: Zielpunkt = weitester Schnitt des Vorausschau-Kreises mit dem Pfad (nur vorwärts im Pfad).
    // Auf dem letzten Abschnitt und näher als die Vorausschau: Endanflug wie move_to.
    bool follow(const Pose& p, double dt_s, MotionOutput& out) {
        const Path& path = cmd_.path;
        const int n = path.size();
        const PathPoint& last = path[n - 1];
        const double L = cfg_.lookahead_in;
        if (n == 1 || (seg_ >= n - 2 && std::hypot(last.x_in - p.x_in, last.y_in - p.y_in) < L))
            return move(p, last.x_in, last.y_in, false, dt_s, out);

        PathPoint target = path[seg_ + 1];
        for (int i = seg_; i <= n - 2; ++i) {
            const PathPoint& a = path[i];
            const PathPoint& b = path[i + 1];
            const double ex = b.x_in - a.x_in, ey = b.y_in - a.y_in;
            const double fx = a.x_in - p.x_in, fy = a.y_in - p.y_in;
            const double qa = ex * ex + ey * ey;
            if (qa < 1e-12)
                continue;
            const double qb = 2.0 * (fx * ex + fy * ey);
            const double qc = fx * fx + fy * fy - L * L;
            const double disc = qb * qb - 4.0 * qa * qc;
            if (disc < 0.0)
                continue;
            const double t = (-qb + std::sqrt(disc)) / (2.0 * qa);
            if (t < 0.0 || t > 1.0)
                continue;
            seg_ = i;
            target = PathPoint{a.x_in + t * ex, a.y_in + t * ey};
        }

        double remaining = std::hypot(path[seg_ + 1].x_in - p.x_in, path[seg_ + 1].y_in - p.y_in);
        for (int j = seg_ + 1; j <= n - 2; ++j)
            remaining += std::hypot(path[j + 1].x_in - path[j].x_in, path[j + 1].y_in - path[j].y_in);
        const double v = std::clamp(lat_.update(remaining, dt_s), -cfg_.follow_max_volts, cfg_.follow_max_volts);

        const double h = deg_to_rad(p.heading_deg);
        const double gx = target.x_in - p.x_in, gy = target.y_in - p.y_in;
        const double lx = gx * std::cos(h) + gy * std::sin(h);
        const double ly = -gx * std::sin(h) + gy * std::cos(h);
        const double l2 = lx * lx + ly * ly;
        const double k = l2 > 1e-9 ? 2.0 * ly / l2 : 0.0;  // Krümmung in 1/Zoll, links positiv
        double left = v * (1.0 - k * cfg_.track_width_in / 2.0);
        double right = v * (1.0 + k * cfg_.track_width_in / 2.0);
        const double peak = std::max(std::fabs(left), std::fabs(right));
        if (peak > cfg_.max_volts) {
            left *= cfg_.max_volts / peak;
            right *= cfg_.max_volts / peak;
        }
        out.left_v = left;
        out.right_v = right;
        return false;
    }

    // Festgefahren: im ganzen Fenster mindestens blocked_min_effort_v und dabei kaum Weg und kaum Drehung.
    bool stuck(const Pose& p, double now_s, double effort_v) {
        if (!window_open_) {
            window_open_ = true;
            window_start_s_ = now_s;
            window_pose_ = p;
            window_min_effort_v_ = effort_v;
            return false;
        }
        window_min_effort_v_ = std::min(window_min_effort_v_, effort_v);
        if (now_s - window_start_s_ < cfg_.blocked_window_s)
            return false;
        const double moved_in = std::hypot(p.x_in - window_pose_.x_in, p.y_in - window_pose_.y_in);
        const double turned_deg = std::fabs(wrap_deg(p.heading_deg - window_pose_.heading_deg));
        const bool blocked = window_min_effort_v_ >= cfg_.blocked_min_effort_v &&
                             moved_in < cfg_.blocked_min_motion_in && turned_deg < cfg_.blocked_min_motion_deg;
        window_start_s_ = now_s;
        window_pose_ = p;
        window_min_effort_v_ = effort_v;
        return blocked;
    }

    MotionConfig cfg_;
    Regulator& lat_;
    Regulator& ang_;
    StateMachine<MotionState> sm_;
    MotionCommand cmd_;
    int seg_ = 0;
    bool approached_ = false;  // move_to: Ziel schon einmal näher als near_in
    double last_s_ = 0.0;
    bool has_last_ = false;
    bool settling_ = false;
    double settle_since_s_ = 0.0;
    bool window_open_ = false;
    double window_start_s_ = 0.0;
    Pose window_pose_;
    double window_min_effort_v_ = 0.0;
};

}  // namespace frostbyte
