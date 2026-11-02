#include <cmath>
#include <cstring>
#include <limits>
#include <string>

#include "config/runner.hpp"
#include "config/stacker.hpp"
#include "control/pid.hpp"
#include "control/tuning.hpp"
#include "control/tuning_store.hpp"
#include "doctest/doctest.h"
#include "hal/sim/sim_storage.hpp"

using namespace frostbyte;

namespace {

// Testwerte, unabhängig von config/.
TuningSet base_set() {
    TuningSet s;
    s.lateral = PidGains{.kp = 1.0, .ki = 0.0, .kd = 0.08, .i_zone = 0.0, .i_max_v = 2.0, .out_max_v = 12.0};
    s.angular = PidGains{.kp = 0.15, .ki = 0.5, .kd = 0.01, .i_zone = 10.0, .i_max_v = 2.0, .out_max_v = 12.0};
    return s;
}

ControllerInput press(Button b) {
    ControllerInput in;
    in.set(b);
    return in;
}

// Eine Taste einmal drücken (Flanke) und wieder loslassen.
void tap(TuningMode& m, Button b, double& t) {
    m.update(press(b), t += 0.01);
    m.update(ControllerInput{}, t += 0.01);
}

}  // namespace

TEST_CASE("Tuning: Serialisieren und Parsen ergeben dieselben Werte (Round-Trip)") {
    TuningSet in = base_set();
    in.lateral.kp = 1.2345;
    in.angular.i_zone = 7.5;
    char buf[512];
    const int n = serialize_tuning(in, buf, sizeof(buf));
    REQUIRE(n > 0);
    REQUIRE(n < static_cast<int>(sizeof(buf)));

    TuningSet out = base_set();
    out.lateral.kp = 99.0;  // wird überschrieben
    CHECK(parse_tuning(buf, static_cast<std::size_t>(n), out));
    CHECK(out.lateral.kp == doctest::Approx(1.2345));
    CHECK(out.lateral.kd == doctest::Approx(0.08));
    CHECK(out.angular.i_zone == doctest::Approx(7.5));
    CHECK(out.angular.ki == doctest::Approx(0.5));
}

TEST_CASE("Tuning: Serialisieren in zu kleinen Puffer meldet Fehler statt abzuschneiden") {
    char buf[16];
    CHECK(serialize_tuning(base_set(), buf, sizeof(buf)) < 0);
}

TEST_CASE("Tuning: kaputte Dateien werden ganz verworfen, die Zielwerte bleiben unverändert") {
    TuningSet good = base_set();
    char buf[512];
    const int n = serialize_tuning(good, buf, sizeof(buf));
    REQUIRE(n > 0);
    const std::string text(buf, static_cast<std::size_t>(n));

    auto rejected = [&](const std::string& s) {
        TuningSet target = base_set();
        target.lateral.kp = 3.0;
        const bool ok = parse_tuning(s.data(), s.size(), target);
        return !ok && target.lateral.kp == 3.0 && target.angular.kd == doctest::Approx(0.01);
    };

    SUBCASE("leer") { CHECK(rejected("")); }
    SUBCASE("abgeschnitten") { CHECK(rejected(text.substr(0, text.size() / 2))); }
    SUBCASE("falsche Version") {
        std::string s = text;
        s.replace(s.find("v1"), 2, "v2");
        CHECK(rejected(s));
    }
    SUBCASE("NaN") {
        std::string s = text;
        s.replace(s.find("lat.kp=") + 7, 6, "nan   ");
        CHECK(rejected(s));
    }
    SUBCASE("negativer Wert") {
        std::string s = text;
        s.replace(s.find("lat.kp=") + 7, 1, "-");
        CHECK(rejected(s));
    }
    SUBCASE("unbekannter Schlüssel") { CHECK(rejected(text + "lat.foo=1.0\n")); }
    SUBCASE("Müll nach dem Wert") {
        std::string s = text;
        const auto p = s.find("lat.kp=");
        s.insert(s.find('\n', p), "x");
        CHECK(rejected(s));
    }
    SUBCASE("Schlüssel doppelt") { CHECK(rejected(text + "lat.kp=1.0\n")); }
    SUBCASE("Schlüssel fehlt") {
        std::string s = text;
        const auto p = s.find("ang.kd=");
        s.erase(p, s.find('\n', p) - p + 1);
        CHECK(rejected(s));
    }
}

TEST_CASE("Tuning: Wert über der Obergrenze wird abgelehnt") {
    TuningSet s = base_set();
    s.lateral.kp = tuning_param(0).max_value * 2.0;
    char buf[512];
    const int n = serialize_tuning(s, buf, sizeof(buf));
    REQUIRE(n > 0);
    TuningSet target = base_set();
    CHECK_FALSE(parse_tuning(buf, static_cast<std::size_t>(n), target));
}

TEST_CASE("Tuning: Speichern und Laden über den Speicher überlebt einen Neustart") {
    hal::SimStorage store;
    TuningSet defaults = base_set();

    TuningSet loaded = defaults;
    CHECK(load_tuning(store, loaded) == LoadResult::missing);
    CHECK(loaded.lateral.kp == doctest::Approx(1.0));

    TuningSet tuned = defaults;
    tuned.lateral.kp = 2.5;
    tuned.angular.kd = 0.03;
    REQUIRE(save_tuning(store, tuned));

    TuningSet after_restart = defaults;  // frischer Start: Standardwerte aus config/
    CHECK(load_tuning(store, after_restart) == LoadResult::loaded);
    CHECK(after_restart.lateral.kp == doctest::Approx(2.5));
    CHECK(after_restart.angular.kd == doctest::Approx(0.03));
    // Was nicht getunt wird (Grenzen aus config/), bleibt aus den Standardwerten.
    CHECK(after_restart.lateral.out_max_v == doctest::Approx(12.0));
    CHECK(after_restart.angular.i_max_v == doctest::Approx(2.0));
}

TEST_CASE("Tuning: kaputter oder nicht schreibbarer Speicher → Standardwerte, kein Absturz") {
    hal::SimStorage store;
    const char junk[] = "kaputt\n";
    REQUIRE(store.write(junk, sizeof(junk) - 1));
    TuningSet t = base_set();
    CHECK(load_tuning(store, t) == LoadResult::invalid);
    CHECK(t.lateral.kp == doctest::Approx(1.0));

    store.set_fail_writes(true);
    CHECK_FALSE(save_tuning(store, base_set()));
}

TEST_CASE("Tuning: Parameter-Tabelle ist vollständig und die Standardwerte liegen in den Grenzen") {
    for (int i = 0; i < tuning_param_count; ++i) {
        const TuningParam& p = tuning_param(i);
        CHECK(std::strlen(p.key) > 0);
        CHECK(p.min_value >= 0.0);
        CHECK(p.max_value > p.min_value);
        CHECK(p.step_fine > 0.0);
        CHECK(p.step_coarse > p.step_fine);
        for (int j = i + 1; j < tuning_param_count; ++j)
            CHECK(std::strcmp(p.key, tuning_param(j).key) != 0);
    }
    for (const TuningSet& s : {TuningSet{config::stacker::lateral_pid, config::stacker::angular_pid},
                               TuningSet{config::runner::lateral_pid, config::runner::angular_pid}}) {
        for (int i = 0; i < tuning_param_count; ++i) {
            const double v = tuning_get(s, i);
            CHECK(v >= tuning_param(i).min_value);
            CHECK(v <= tuning_param(i).max_value);
        }
    }
}

TEST_CASE("TuningMode: ist standardmäßig aus und ignoriert alle Tasten") {
    TuningMode m(base_set(), false);
    double t = 0.0;
    tap(m, Button::right, t);
    tap(m, Button::a, t);
    CHECK(m.values().lateral.kp == doctest::Approx(1.0));
    CHECK_FALSE(m.take_save_request());
    char ev[64];
    CHECK_FALSE(m.take_event(ev, sizeof(ev)));
}

TEST_CASE("TuningMode: Auswahl, Ändern, Schrittweite, Grenzen") {
    TuningMode m(base_set(), true);
    double t = 0.0;
    CHECK(m.state().index == 0);  // lat.kp
    tap(m, Button::right, t);
    CHECK(m.values().lateral.kp == doctest::Approx(1.0 + tuning_param(0).step_fine));
    tap(m, Button::left, t);
    tap(m, Button::left, t);
    CHECK(m.values().lateral.kp == doctest::Approx(1.0 - tuning_param(0).step_fine));

    tap(m, Button::x, t);  // grobe Schritte
    CHECK(m.state().coarse);
    tap(m, Button::right, t);
    CHECK(m.values().lateral.kp == doctest::Approx(1.0 - tuning_param(0).step_fine + tuning_param(0).step_coarse));

    tap(m, Button::down, t);
    CHECK(m.state().index == 1);
    tap(m, Button::up, t);
    tap(m, Button::up, t);  // umlaufend
    CHECK(m.state().index == tuning_param_count - 1);

    // nie unter min, nie über max
    for (int i = 0; i < 2000; ++i)
        tap(m, Button::left, t);
    CHECK(tuning_get(m.values(), m.state().index) == doctest::Approx(tuning_param(m.state().index).min_value));
    for (int i = 0; i < 20000; ++i)
        tap(m, Button::right, t);
    CHECK(tuning_get(m.values(), m.state().index) == doctest::Approx(tuning_param(m.state().index).max_value));
}

TEST_CASE("TuningMode: gehaltene Taste wirkt nur einmal (Flanke)") {
    TuningMode m(base_set(), true);
    m.update(press(Button::right), 0.01);
    m.update(press(Button::right), 0.02);
    m.update(press(Button::right), 0.03);
    CHECK(m.values().lateral.kp == doctest::Approx(1.0 + tuning_param(0).step_fine));
}

TEST_CASE("TuningMode: Speichern wird nur angefordert, nicht ausgeführt; Ereignis für das Log") {
    TuningMode m(base_set(), true);
    double t = 0.0;
    tap(m, Button::right, t);
    char ev[64];
    REQUIRE(m.take_event(ev, sizeof(ev)));
    CHECK(std::string(ev).rfind("TUNE:lat.kp=", 0) == 0);
    CHECK(std::strchr(ev, ',') == nullptr);
    CHECK(std::strchr(ev, ';') == nullptr);
    CHECK_FALSE(m.take_event(ev, sizeof(ev)));  // nur einmal

    CHECK_FALSE(m.take_save_request());
    tap(m, Button::a, t);
    CHECK(m.take_save_request());
    CHECK_FALSE(m.take_save_request());  // verbraucht
}

TEST_CASE("TuningMode: B verwirft Änderungen seit dem Start bzw. letzten Speichern") {
    TuningMode m(base_set(), true);
    double t = 0.0;
    tap(m, Button::right, t);
    tap(m, Button::right, t);
    CHECK(m.state().dirty);
    tap(m, Button::b, t);
    CHECK(m.values().lateral.kp == doctest::Approx(1.0));
    CHECK_FALSE(m.state().dirty);
}

TEST_CASE("Pid: set_gains im Betrieb lässt den Ausgang nicht springen und beachtet die neue I-Grenze") {
    PidGains g{.kp = 0.0, .ki = 1.0, .kd = 0.0, .i_zone = 0.0, .i_max_v = 5.0, .out_max_v = 12.0};
    Pid pid(g);
    for (int i = 0; i < 100; ++i)
        pid.update(1.0, 0.01);
    const double before = pid.update(0.0, 0.01);
    CHECK(before == doctest::Approx(1.0));

    g.ki = 4.0;  // ki verändern: der aufgelaufene I-Anteil (schon mit ki multipliziert) bleibt
    pid.set_gains(g);
    CHECK(pid.update(0.0, 0.01) == doctest::Approx(before));

    g.i_max_v = 0.5;  // engere Grenze: gekappt
    pid.set_gains(g);
    CHECK(pid.update(0.0, 0.01) == doctest::Approx(0.5));
}

TEST_CASE("apply_tuning: schreibt die Werte in beide Regler") {
    TuningSet s = base_set();
    s.lateral.kp = 2.0;
    s.angular.kp = 0.3;
    Pid lat(base_set().lateral), ang(base_set().angular);
    apply_tuning(s, lat, ang);
    CHECK(lat.update(1.0, 0.01) == doctest::Approx(2.0));  // erster Aufruf: kein D-Sprung, ki = 0
    CHECK(ang.update(10.0, 0.0) == doctest::Approx(3.0));
}

TEST_CASE("Tuning ist in den ausgelieferten Konfigurationen aus") {
    CHECK_FALSE(config::stacker::tuning_enabled);
    CHECK_FALSE(config::runner::tuning_enabled);
}
