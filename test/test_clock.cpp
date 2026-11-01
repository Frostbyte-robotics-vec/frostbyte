#include "core/clock.hpp"
#include "core/snapshot.hpp"
#include "core/units.hpp"
#include "doctest/doctest.h"

using namespace frostbyte;

TEST_CASE("FakeClock: Zeit nur durch advance") {
    FakeClock c;
    CHECK(c.now_s() == doctest::Approx(0.0));
    c.advance_s(0.01);
    c.advance_s(0.01);
    CHECK(c.now_s() == doctest::Approx(0.02));
}

TEST_CASE("Snapshot: Zyklus zählt hoch, dt aus der Uhr") {
    FakeClock c;
    SnapshotBuilder b(c);
    c.advance_s(0.01);
    Snapshot s1 = b.next();
    c.advance_s(0.012);
    Snapshot s2 = b.next();
    CHECK(s1.cycle == 1);
    CHECK(s2.cycle == 2);
    CHECK(s1.dt_s == doctest::Approx(0.01));
    CHECK(s2.dt_s == doctest::Approx(0.012));
    CHECK(s2.t_s == doctest::Approx(0.022));
}

TEST_CASE("Einheiten: mm und ms") {
    CHECK(mm_to_in(25.4) == doctest::Approx(1.0));
    CHECK(in_to_mm(2.0) == doctest::Approx(50.8));
    CHECK(ms_to_s(1500.0) == doctest::Approx(1.5));
    CHECK(s_to_ms(0.01) == doctest::Approx(10.0));
    CHECK(deg_to_rad(180.0) == doctest::Approx(pi));
}
