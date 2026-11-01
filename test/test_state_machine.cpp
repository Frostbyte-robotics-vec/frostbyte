#include "core/state_machine.hpp"
#include "doctest/doctest.h"

using frostbyte::StateMachine;

namespace {
enum class S { Leer, Greift, Fehler };
}

TEST_CASE("StateMachine: Startzustand und Wechsel") {
    StateMachine<S> sm(S::Leer, 0.0);
    CHECK(sm.state() == S::Leer);
    sm.enter(S::Greift, 1.0);
    CHECK(sm.state() == S::Greift);
    CHECK(sm.elapsed_s(1.5) == doctest::Approx(0.5));
}

TEST_CASE("StateMachine: Timeout führt in den Ausweichzustand") {
    StateMachine<S> sm(S::Leer, 0.0);
    sm.enter(S::Greift, 10.0, 1.5, S::Fehler);
    CHECK_FALSE(sm.update(11.0));
    CHECK(sm.state() == S::Greift);
    CHECK_FALSE(sm.update(11.49));
    CHECK(sm.update(11.5));
    CHECK(sm.state() == S::Fehler);
    CHECK(sm.timed_out());
    CHECK(sm.elapsed_s(12.0) == doctest::Approx(0.5));
}

TEST_CASE("StateMachine: Timeout feuert nur einmal") {
    StateMachine<S> sm(S::Leer, 0.0);
    sm.enter(S::Greift, 0.0, 1.0, S::Leer);
    CHECK(sm.update(2.0));
    CHECK_FALSE(sm.update(100.0));
    CHECK(sm.state() == S::Leer);
}

TEST_CASE("StateMachine: Zustand ohne Timeout bleibt beliebig lange") {
    StateMachine<S> sm(S::Leer, 0.0);
    CHECK_FALSE(sm.update(1e9));
    CHECK(sm.state() == S::Leer);
    CHECK_FALSE(sm.timed_out());
}

TEST_CASE("StateMachine: neuer Wechsel setzt timed_out zurück") {
    StateMachine<S> sm(S::Leer, 0.0);
    sm.enter(S::Greift, 0.0, 1.0, S::Fehler);
    sm.update(1.0);
    REQUIRE(sm.timed_out());
    sm.enter(S::Leer, 2.0);
    CHECK_FALSE(sm.timed_out());
}
