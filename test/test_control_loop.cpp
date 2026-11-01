#include "core/control_loop.hpp"
#include "doctest/doctest.h"

using namespace frostbyte;

TEST_CASE("PhaseFlag: Start ist DISABLED, Callbacks setzen nur die Phase") {
    PhaseFlag p;
    CHECK(p.get() == Phase::Disabled);
    p.set(Phase::Autonomous);
    CHECK(p.get() == Phase::Autonomous);
    p.set(Phase::Driver);
    CHECK(p.get() == Phase::Driver);
}

TEST_CASE("CycleStats: puenktlicher Takt ergibt Jitter 0") {
    CycleStats s(0.010);
    for (int i = 0; i < 100; ++i) {
        s.tick_start(i * 0.010, i * 0.010);
        s.tick_end(i * 0.010 + 0.002);
    }
    CHECK(s.jitter_max_s() == doctest::Approx(0.0));
    CHECK(s.work_max_s() == doctest::Approx(0.002));
    CHECK(s.overruns() == 0);
}

TEST_CASE("CycleStats: Jitter ist die Verspaetung gegen den angeforderten Weckzeitpunkt") {
    CycleStats s(0.010);
    s.tick_start(0.0, 0.0);
    s.tick_end(0.001);
    s.tick_start(0.0108, 0.010);  // 0,8 ms zu spaet
    s.tick_end(0.0118);
    s.tick_start(0.0260, 0.020);  // 6 ms zu spaet (verpasster Takt wird nicht weggerundet)
    s.tick_end(0.0270);
    CHECK(s.jitter_max_s() == doctest::Approx(0.006));
}

TEST_CASE("CycleStats: Ueberzug zaehlt, Folge-Zaehler setzt bei gutem Zyklus zurueck") {
    CycleStats s(0.010);
    s.tick_start(0.0, 0.0);
    s.tick_end(0.012);
    CHECK(s.overruns() == 1);
    CHECK(s.consecutive_overruns() == 1);
    s.tick_start(0.020, 0.020);
    s.tick_end(0.033);
    CHECK(s.consecutive_overruns() == 2);
    s.tick_start(0.040, 0.040);
    s.tick_end(0.041);
    CHECK(s.consecutive_overruns() == 0);
    CHECK(s.overruns() == 2);
    CHECK_FALSE(s.warning());
}

TEST_CASE("CycleStats: Warnung ab 3 Ueberzuegen in Folge") {
    CycleStats s(0.010);
    for (int i = 0; i < 3; ++i) {
        s.tick_start(i * 0.02, i * 0.02);
        s.tick_end(i * 0.02 + 0.015);
    }
    CHECK(s.warning());
}

TEST_CASE("ControlLoop: erster Takt zur Startzeit, danach festes Raster ohne Drift") {
    ControlLoop loop(0.010, 5.0);
    CHECK(loop.next_wake_s() == doctest::Approx(5.0));
    loop.begin(5.0);
    loop.end(5.004);  // 4 ms Rechenzeit verschieben das Raster nicht
    CHECK(loop.next_wake_s() == doctest::Approx(5.010));
    loop.begin(5.0103);
    loop.end(5.0143);
    CHECK(loop.next_wake_s() == doctest::Approx(5.020));
    CHECK(loop.stats().jitter_max_s() == doctest::Approx(0.0003));
}

TEST_CASE("ControlLoop: nach langem Zyklus wird uebersprungen, nicht nachgeholt") {
    ControlLoop loop(0.010, 0.0);
    loop.begin(0.0);
    loop.end(0.025);
    CHECK(loop.next_wake_s() == doctest::Approx(0.030));
}

TEST_CASE("ControlLoop: Ende exakt auf der Rastergrenze: naechster Takt ist der folgende, nicht ein bereits faelliger") {
    ControlLoop loop(0.010, 0.0);
    loop.begin(0.0);
    loop.end(0.010);
    CHECK(loop.next_wake_s() == doctest::Approx(0.020));
}
