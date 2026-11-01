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

TEST_CASE("CycleStats: exakter Takt ergibt Jitter 0") {
    CycleStats s(0.010);
    for (int i = 0; i < 100; ++i) {
        s.tick_start(i * 0.010);
        s.tick_end(i * 0.010 + 0.002);
    }
    CHECK(s.jitter_max_s() == doctest::Approx(0.0));
    CHECK(s.work_max_s() == doctest::Approx(0.002));
    CHECK(s.overruns() == 0);
}

TEST_CASE("CycleStats: Jitter ist die Abweichung vom festen Raster, nicht von der letzten Periode") {
    CycleStats s(0.010);
    s.tick_start(0.0);
    s.tick_end(0.001);
    s.tick_start(0.0108);  // 0,8 ms zu spät
    s.tick_end(0.0118);
    s.tick_start(0.0200);  // zurück im Raster
    s.tick_end(0.0210);
    CHECK(s.jitter_max_s() == doctest::Approx(0.0008));
}

TEST_CASE("CycleStats: Überzug zählt, Folge-Zähler setzt bei gutem Zyklus zurück") {
    CycleStats s(0.010);
    s.tick_start(0.0);
    s.tick_end(0.012);  // länger als der Takt
    CHECK(s.overruns() == 1);
    CHECK(s.consecutive_overruns() == 1);
    s.tick_start(0.012);
    s.tick_end(0.025);
    CHECK(s.consecutive_overruns() == 2);
    s.tick_start(0.030);
    s.tick_end(0.031);
    CHECK(s.consecutive_overruns() == 0);
    CHECK(s.overruns() == 2);
    CHECK_FALSE(s.warning());
}

TEST_CASE("CycleStats: Warnung ab 3 Überzügen in Folge") {
    CycleStats s(0.010);
    for (int i = 0; i < 3; ++i) {
        s.tick_start(i * 0.02);
        s.tick_end(i * 0.02 + 0.015);
    }
    CHECK(s.warning());
}

TEST_CASE("ControlLoop: next_wake liegt auf festem Raster ohne Drift") {
    ControlLoop loop(0.010, 5.0);
    CHECK(loop.next_wake_s() == doctest::Approx(5.010));
    // Ein Zyklus, der 4 ms braucht, verschiebt den nächsten Weckzeitpunkt nicht.
    loop.begin(5.0);
    loop.end(5.004);
    CHECK(loop.next_wake_s() == doctest::Approx(5.010));
}

TEST_CASE("ControlLoop: Führt ein Subsystem-Takt pro Raster aus, auch nach langem Zyklus") {
    ControlLoop loop(0.010, 0.0);
    loop.begin(0.0);
    loop.end(0.025);  // 2,5 Takte verpasst
    // Raster bleibt erhalten: nächster Wecker ist die nächste Rastergrenze in der Zukunft
    CHECK(loop.next_wake_s() == doctest::Approx(0.030));
}
