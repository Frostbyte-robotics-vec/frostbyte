#include <fstream>
#include <set>
#include <sstream>
#include <string>

#include "config/runner.hpp"
#include "config/stacker.hpp"
#include "controls_doc.hpp"
#include "doctest/doctest.h"
#include "match/driver_map.hpp"

using namespace frostbyte;

namespace {

// Testwerte, unabhängig von config/.
const Binding kBindings[] = {
    {Action::grab, Button::r1}, {Action::place, Button::r2},        {Action::slow, Button::l2},
    {Action::hold, Button::b},  {Action::assist_toggle, Button::x},
};
const DriverProfile kProfiles[] = {
    {"p0", {.deadband = 0.1, .exponent = 2.0, .max_speed = 1.0}},
    {"p1", {.deadband = 0.1, .exponent = 3.0, .max_speed = 0.5}},
};

DriverMap test_map() { return DriverMap(kBindings, 5, kProfiles, 2); }

ControllerInput press(Button b) {
    ControllerInput in;
    in.set(b);
    return in;
}

bool has_action(const Binding* list, int n, Action a) {
    for (int i = 0; i < n; ++i)
        if (list[i].action == a)
            return true;
    return false;
}

}  // namespace

TEST_CASE("Grundbelegung (Fahren, Greifen, Setzen) ist auf beiden Robotern identisch") {
    for (Action a : {Action::grab, Action::place, Action::slow, Action::hold}) {
        const Binding* s = nullptr;
        const Binding* r = nullptr;
        for (int i = 0; i < config::stacker::bindings_count; ++i)
            if (config::stacker::bindings[i].action == a)
                s = &config::stacker::bindings[i];
        for (int i = 0; i < config::runner::bindings_count; ++i)
            if (config::runner::bindings[i].action == a)
                r = &config::runner::bindings[i];
        REQUIRE(s != nullptr);
        REQUIRE(r != nullptr);
        CHECK(s->button == r->button);
    }
}

TEST_CASE("Jedes Makro hat genau eine Taste, keine Taste ist doppelt belegt") {
    for (Action a : {Action::grab, Action::place, Action::rotate_cup, Action::endgame, Action::toggle_place})
        CHECK(has_action(config::stacker::bindings, config::stacker::bindings_count, a));
    CHECK(has_action(config::stacker::bindings, config::stacker::bindings_count, Action::assist_toggle));

    auto unique = [](const Binding* list, int n) {
        std::set<int> buttons;
        std::set<int> actions;
        for (int i = 0; i < n; ++i) {
            CHECK(buttons.insert(static_cast<int>(list[i].button)).second);
            CHECK(actions.insert(static_cast<int>(list[i].action)).second);
        }
    };
    unique(config::stacker::bindings, config::stacker::bindings_count);
    unique(config::runner::bindings, config::runner::bindings_count);
}

TEST_CASE("Taste löst ihre Aktion einmal pro Drücken aus (Flanke)") {
    DriverMap m = test_map();
    Button grab_btn = kBindings[0].button;
    Action grab_act = kBindings[0].action;
    ControllerInput in = press(grab_btn);
    CHECK(m.update(in).pressed(grab_act));
    CHECK_FALSE(m.update(in).pressed(grab_act));  // gehalten
    in.set(grab_btn, false);
    CHECK_FALSE(m.update(in).pressed(grab_act));
    in.set(grab_btn, true);
    CHECK(m.update(in).pressed(grab_act));  // neu gedrückt
}

TEST_CASE("Langsam und Hold gelten, solange die Taste gehalten wird") {
    DriverMap m = test_map();
    Button slow_btn{};
    Button hold_btn{};
    for (int i = 0; i < 5; ++i) {
        if (kBindings[i].action == Action::slow)
            slow_btn = kBindings[i].button;
        if (kBindings[i].action == Action::hold)
            hold_btn = kBindings[i].button;
    }
    CHECK_FALSE(m.update(ControllerInput{}).drive.slow);
    CHECK(m.update(press(slow_btn)).drive.slow);
    CHECK(m.update(press(slow_btn)).drive.slow);  // weiter gehalten
    CHECK_FALSE(m.update(ControllerInput{}).drive.slow);
    CHECK(m.update(press(hold_btn)).drive.mode == DriveMode::hold);
}

TEST_CASE("Sticks: links vor/zurück, rechts drehen (rechts = im Uhrzeigersinn = turn < 0)") {
    DriverMap m = test_map();
    ControllerInput in;
    in.left_y = 0.8;
    in.right_x = 0.5;
    DriverIntent i = m.update(in);
    CHECK(i.drive.mode == DriveMode::arcade);
    CHECK(i.drive.forward == doctest::Approx(0.8));
    CHECK(i.drive.turn == doctest::Approx(-0.5));
}

TEST_CASE("Manuelle Eingabe übernimmt sofort, auch während eine Makro-Taste gedrückt ist") {
    DriverMap m = test_map();
    ControllerInput in = press(kBindings[0].button);
    CHECK_FALSE(m.update(in).manual_drive);  // Makro allein: kein manueller Eingriff
    in.set(kBindings[0].button, false);
    in.set(kBindings[1].button);
    in.left_y = 0.5;
    DriverIntent i = m.update(in);
    CHECK(i.manual_drive);
    CHECK(i.drive.mode == DriveMode::arcade);
    CHECK(i.pressed(kBindings[1].action));
}

TEST_CASE("Stick innerhalb der Totzone zählt nicht als manueller Eingriff") {
    DriverMap m = test_map();
    ControllerInput in;
    in.left_y = m.drive_config().deadband * 0.5;
    CHECK_FALSE(m.update(in).manual_drive);
}

TEST_CASE("Hold gehalten, aber Stick außerhalb der Totzone: Fahren übernimmt") {
    DriverMap m = test_map();
    ControllerInput in = press(Button::b);
    in.left_y = 0.5;
    DriverIntent i = m.update(in);
    CHECK(i.drive.mode == DriveMode::arcade);
    CHECK(i.manual_drive);
}

TEST_CASE("Assistenz ist standardmäßig aus und nur per Taste umschaltbar") {
    DriverMap m = test_map();
    Button b{};
    for (int i = 0; i < 5; ++i)
        if (kBindings[i].action == Action::assist_toggle)
            b = kBindings[i].button;
    CHECK_FALSE(m.update(ControllerInput{}).assist_enabled);
    CHECK(m.update(press(b)).assist_enabled);
    CHECK(m.update(press(b)).assist_enabled);  // gehalten schaltet nicht wieder um
    CHECK(m.update(ControllerInput{}).assist_enabled);
    CHECK_FALSE(m.update(press(b)).assist_enabled);
}

TEST_CASE("Fahrerprofil: Kurve und Höchstgeschwindigkeit wählbar") {
    DriverMap m = test_map();
    m.select_profile(0);
    DriveConfig p0 = m.drive_config();
    m.select_profile(1);
    CHECK(m.profile_index() == 1);
    CHECK((m.drive_config().max_speed != p0.max_speed || m.drive_config().exponent != p0.exponent));
    m.select_profile(99);  // außerhalb: auf letztes Profil begrenzt
    CHECK(m.profile_index() == 2 - 1);
}

TEST_CASE("Endgame-Vibration bei 15 s und 10 s Restzeit, je einmal pro Überschreiten") {
    CHECK_FALSE(endgame_rumble(20.0, 16.0));
    CHECK(endgame_rumble(15.2, 14.9));
    CHECK_FALSE(endgame_rumble(14.9, 14.5));
    CHECK(endgame_rumble(10.01, 9.99));
    CHECK_FALSE(endgame_rumble(9.9, 5.0));
    CHECK(endgame_rumble(15.0 + 0.001, 15.0));  // genau auf der Schwelle
    CHECK(endgame_rumble(16.0, 12.0));          // Sprung über 15 s
    CHECK(endgame_rumble(12.0, 8.0));           // Sprung über 10 s
    CHECK_FALSE(endgame_rumble(10.0, 10.0));    // keine Zeit vergangen
    CHECK_FALSE(endgame_rumble(8.0, 12.0));     // Uhr läuft nie rückwärts
}

TEST_CASE("docs/controls.md stimmt mit der Belegung in config/ überein (Export)") {
    std::ifstream f(std::string(FB_ROOT_DIR) + "/docs/controls.md");
    REQUIRE(f.good());
    std::stringstream ss;
    ss << f.rdbuf();
    CHECK(ss.str() == controls_markdown());
}
