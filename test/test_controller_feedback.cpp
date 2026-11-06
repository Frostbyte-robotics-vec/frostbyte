#include <string>

#include "doctest/doctest.h"
#include "hal/sim/sim_controller_output.hpp"
#include "match/controller_feedback.hpp"

using namespace frostbyte;

namespace {

// Testwerte, unabhängig von config/.
constexpr double kDuration = 60.0;

}  // namespace

TEST_CASE("Controller: Vibration genau einmal bei 15 s und einmal bei 10 s") {
    hal::SimControllerOutput out;
    ControllerFeedback fb(out, kDuration);
    double first_s = -1.0, second_s = -1.0;
    for (int i = 0; i <= 6000; ++i) {
        const double t = i * 0.01;
        fb.update(t, "x");
        if (out.rumbles.size() == 1 && first_s < 0.0)
            first_s = t;
        if (out.rumbles.size() == 2 && second_s < 0.0)
            second_s = t;
    }
    REQUIRE(out.rumbles.size() == 2);
    CHECK(first_s == doctest::Approx(45.0).epsilon(0.001));
    CHECK(second_s == doctest::Approx(50.0).epsilon(0.001));
    CHECK(out.rumbles[0] != out.rumbles[1]);  // 15 s und 10 s sind unterscheidbar
}

TEST_CASE("Controller: ein Sprung über beide Schwellen vibriert einmal, nicht zweimal pro Zyklus") {
    hal::SimControllerOutput out;
    ControllerFeedback fb(out, kDuration);
    fb.update(0.0, "x");
    fb.update(52.0, "x");  // 8 s Restzeit: beide Schwellen übersprungen
    CHECK(out.rumbles.size() == 1);
}

TEST_CASE("Controller: keine Vibration beim ersten Aufruf nach einer Schwelle (keine Vorgängerzeit bekannt)") {
    hal::SimControllerOutput out;
    ControllerFeedback fb(out, kDuration);
    fb.update(55.0, "x");  // Einstieg mitten im Lauf: keine Vibration für etwas, das nicht gesehen wurde
    CHECK(out.rumbles.empty());
}

TEST_CASE("Controller: reset beginnt einen neuen Lauf, Schwellen gelten wieder") {
    hal::SimControllerOutput out;
    ControllerFeedback fb(out, kDuration);
    fb.update(0.0, "x");
    fb.update(46.0, "x");
    fb.reset();
    fb.update(0.0, "x");
    fb.update(46.0, "x");
    CHECK(out.rumbles.size() == 2);
}

TEST_CASE("Controller: Anzeige zeigt Restzeit und Status, sendet nur bei Änderung und höchstens alle 100 ms") {
    hal::SimControllerOutput out;
    ControllerFeedback fb(out, kDuration);
    fb.update(0.00, "P1");
    REQUIRE(out.prints.size() == 1);
    CHECK(out.prints[0].line == 0);
    CHECK(out.prints[0].text == "60s P1         ");  // auf 15 Zeichen aufgefüllt
    fb.update(0.01, "P1");                           // unverändert
    fb.update(0.05, "P2");                           // geändert, aber zu früh
    CHECK(out.prints.size() == 1);
    fb.update(0.11, "P2");  // jetzt darf es raus
    REQUIRE(out.prints.size() == 2);
    CHECK(out.prints[1].text == "60s P2         ");
    fb.update(1.20, "P2");  // Restzeit 59 s
    REQUIRE(out.prints.size() == 3);
    CHECK(out.prints[2].text == "59s P2         ");
}

TEST_CASE("Controller: Text passt auf eine Controllerzeile (genau 15 Zeichen), Restzeit nie negativ") {
    hal::SimControllerOutput out;
    ControllerFeedback fb(out, kDuration);
    fb.update(70.0, "ein sehr langer Statustext");
    REQUIRE(out.prints.size() == 1);
    CHECK(out.prints[0].text.size() == 15);
    CHECK(out.prints[0].text.rfind("0s", 0) == 0);
}

TEST_CASE("Controller: abgelehntes Senden der Anzeige wird wiederholt") {
    hal::SimControllerOutput out;
    out.accept_prints = false;
    ControllerFeedback fb(out, kDuration);
    fb.update(0.0, "P1");
    fb.update(0.2, "P1");
    CHECK(out.prints.empty());
    out.accept_prints = true;
    fb.update(0.4, "P1");
    CHECK(out.prints.size() == 1);
}

TEST_CASE("Controller: von 10s auf 9s bleibt kein Zeichen der alten Anzeige stehen") {
    hal::SimControllerOutput out;
    ControllerFeedback fb(out, kDuration);
    fb.update(50.0, "stacker");  // Einstieg bei 10 s: keine Vorgängerzeit, keine Vibration
    fb.update(51.0, "stacker");  // 9 s
    REQUIRE(out.prints.size() == 2);
    CHECK(out.prints[0].text == "10s stacker    ");
    CHECK(out.prints[1].text == "9s stacker     ");
}

TEST_CASE("Controller: Vibration und Text nie im selben Aufruf, der Text folgt im nächsten") {
    hal::SimControllerOutput out;
    ControllerFeedback fb(out, kDuration);
    fb.update(44.0, "x");
    const std::size_t prints_before = out.prints.size();
    fb.update(45.5, "x");  // 14,5 s: Schwelle 15 s unterschritten
    CHECK(out.rumbles.size() == 1);
    CHECK(out.prints.size() == prints_before);  // in diesem Aufruf nur die Vibration
    fb.update(45.7, "x");
    CHECK(out.prints.size() == prints_before + 1);
    CHECK(out.prints.back().text.rfind("15s", 0) == 0);
}
