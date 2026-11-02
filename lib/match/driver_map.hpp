#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "core/snapshot.hpp"
#include "subsystems/drive.hpp"

// Schicht 2: Fahrer-Belegung (C-03). Reine Abbildung Controller-Eingabe → Absichten (Intents). Ruft keine Subsysteme
// auf; die Makros (Greifen, Setzen, …) setzen die Tickets der jeweiligen Subsysteme um. Belegung und Fahrerprofile
// liegen pro Roboter in config/ (ADR-014). Einheiten: Sticks -1..1, Zeit in Sekunden.
namespace frostbyte {

enum class Action { grab, place, rotate_cup, endgame, toggle_place, assist_toggle, slow, hold };
inline constexpr int action_count = 8;

// Gehalten-Aktionen gelten, solange die Taste gedrückt ist; alle anderen lösen einmal pro Drücken aus.
inline constexpr bool is_held(Action a) { return a == Action::slow || a == Action::hold; }

struct Binding {
    Action action;
    Button button;
};

// Ein Fahrerprofil: Kurve (Totzone, Exponent) und Höchstgeschwindigkeit. Werte sind Tuning, keine Wahrheit.
struct DriverProfile {
    const char* name;
    DriveConfig drive;
};

struct DriverIntent {
    DriveRequest drive;
    bool manual_drive = false;     // Stick außerhalb der Totzone: laufende Fahr-Makros abbrechen
    bool assist_enabled = false;   // Fahrassistenz nur zuschaltbar, startet immer aus
    std::array<bool, action_count> actions{};  // Flanke (Drücken) bzw. gehalten, siehe is_held()

    bool pressed(Action a) const { return actions[static_cast<int>(a)]; }
};

class DriverMap {
public:
    DriverMap(const Binding* bindings, int binding_count, const DriverProfile* profiles, int profile_count)
        : bindings_(bindings), binding_count_(binding_count), profiles_(profiles), profile_count_(profile_count) {}

    void select_profile(int index) { profile_ = std::clamp(index, 0, profile_count_ - 1); }
    int profile_index() const { return profile_; }
    const DriveConfig& drive_config() const { return profiles_[profile_].drive; }

    // Einmal pro Zyklus mit der aktuellen Eingabe aufrufen.
    DriverIntent update(const ControllerInput& in) {
        DriverIntent out;
        for (int i = 0; i < binding_count_; ++i) {
            const Binding& b = bindings_[i];
            const bool now = in.down(b.button);
            const bool before = prev_.down(b.button);
            out.actions[static_cast<int>(b.action)] = is_held(b.action) ? now : (now && !before);
        }
        if (out.pressed(Action::assist_toggle))
            assist_ = !assist_;
        out.assist_enabled = assist_;

        const double db = drive_config().deadband;
        out.manual_drive = std::fabs(in.left_y) > db || std::fabs(in.right_x) > db;
        out.drive.forward = in.left_y;
        out.drive.turn = -in.right_x;  // rechts = im Uhrzeigersinn = negative Drehung
        out.drive.slow = out.pressed(Action::slow);
        out.drive.mode = (out.pressed(Action::hold) && !out.manual_drive) ? DriveMode::hold : DriveMode::arcade;
        prev_ = in;
        return out;
    }

private:
    const Binding* bindings_;
    int binding_count_;
    const DriverProfile* profiles_;
    int profile_count_;
    int profile_ = 0;
    bool assist_ = false;
    ControllerInput prev_;
};

// Controller vibriert bei 15 s und 10 s Restzeit: true im Zyklus, in dem die Restzeit eine der Schwellen erreicht
// oder unterschreitet. Die Uhr läuft nur abwärts; Sprünge über eine Schwelle zählen ebenfalls.
inline bool endgame_rumble(double prev_remaining_s, double remaining_s) {
    for (double threshold_s : {15.0, 10.0})
        if (prev_remaining_s > threshold_s && remaining_s <= threshold_s)
            return true;
    return false;
}

}  // namespace frostbyte
