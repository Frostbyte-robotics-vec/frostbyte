#pragma once

#include <string>

#include "config/runner.hpp"
#include "config/stacker.hpp"
#include "match/driver_map.hpp"

// Erzeugt docs/controls.md aus der Belegung in config/ (C-03). Nur für Laptop-Werkzeug und Test; nicht im Roboter.
namespace frostbyte {

inline const char* button_label(Button b) {
    switch (b) {
        case Button::a: return "A";
        case Button::b: return "B";
        case Button::x: return "X";
        case Button::y: return "Y";
        case Button::up: return "Pfeil hoch";
        case Button::down: return "Pfeil runter";
        case Button::left: return "Pfeil links";
        case Button::right: return "Pfeil rechts";
        case Button::l1: return "L1";
        case Button::l2: return "L2";
        case Button::r1: return "R1";
        case Button::r2: return "R2";
    }
    return "?";
}

inline const char* action_label(Action a) {
    switch (a) {
        case Action::grab: return "Greifen";
        case Action::place: return "Setzen";
        case Action::rotate_cup: return "Cup drehen";
        case Action::endgame: return "Endgame-Modus (Lift einfahren + Hold)";
        case Action::toggle_place: return "Toggle setzen";
        case Action::assist_toggle: return "Fahrassistenz an/aus (startet aus)";
        case Action::slow: return "Langsam-Modus (gehalten)";
        case Action::hold: return "Antrieb HOLD (gehalten)";
    }
    return "?";
}

inline std::string controls_table(const char* robot, const Binding* bindings, int count, const DriverProfile* profiles,
                                  int profile_count) {
    std::string s = std::string("## ") + robot + "\n\n| Taste | Aktion |\n| --- | --- |\n";
    s += "| Linker Stick (hoch/runter) | Fahren vor/zurück |\n| Rechter Stick (links/rechts) | Drehen |\n";
    for (int i = 0; i < count; ++i)
        s += std::string("| ") + button_label(bindings[i].button) + " | " + action_label(bindings[i].action) + " |\n";
    s += "\nFahrerprofile (Tuning, am Roboter mit dem Fahrer einstellen):\n\n| Profil | Totzone | Exponent | Höchstgeschwindigkeit |\n| --- | --- | --- | --- |\n";
    for (int i = 0; i < profile_count; ++i)
        s += std::string("| ") + profiles[i].name + " | " + std::to_string(profiles[i].drive.deadband).substr(0, 4) + " | " +
             std::to_string(profiles[i].drive.exponent).substr(0, 3) + " | " +
             std::to_string(profiles[i].drive.max_speed).substr(0, 4) + " |\n";
    return s + "\n";
}

inline std::string controls_markdown() {
    std::string s =
        "<!-- Erzeugt aus config/stacker.hpp und config/runner.hpp mit frostbyte_controls_export. Nicht von Hand ändern. -->\n"
        "# Fahrerbedienung\n\n"
        "Greifen, Setzen und Fahren liegen auf beiden Robotern auf denselben Tasten. Jede manuelle Fahreingabe bricht "
        "laufende Makros am Antrieb sofort ab. Der Controller vibriert bei 15 s und 10 s Restzeit.\n\n";
    s += controls_table(config::stacker::robot_name, config::stacker::bindings, config::stacker::bindings_count,
                        config::stacker::profiles, config::stacker::profile_count);
    s += controls_table(config::runner::robot_name, config::runner::bindings, config::runner::bindings_count,
                        config::runner::profiles, config::runner::profile_count);
    return s;
}

}  // namespace frostbyte
