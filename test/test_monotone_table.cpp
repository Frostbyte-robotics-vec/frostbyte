#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "doctest/doctest.h"
#include "util/monotone_table.hpp"

using namespace frostbyte;

namespace {
// Testgeometrie eines schwenkenden Lifts: Hoehe = 10 + 20 sin(Winkel), Winkel 0..90 Grad, fuenf gleichmaessige Punkte.
double arm_height(double deg) { return 10.0 + 20.0 * std::sin(deg * 3.14159265358979323846 / 180.0); }
}  // namespace

TEST_CASE("MonotoneTable: fuenf Punkte einer Schwenkarm-Geometrie, Fehler <= 0,3 Zoll (linear waere zu grob)") {
    std::array<double, 5> x{0, 22.5, 45, 67.5, 90}, y{};
    for (int i = 0; i < 5; ++i)
        y[i] = arm_height(x[i]);
    MonotoneTable<5> t;
    REQUIRE(t.set(x, y));
    double max_cubic = 0.0, max_linear = 0.0;
    for (double a = 0.0; a <= 90.0; a += 0.05) {
        max_cubic = std::max(max_cubic, std::fabs(t.eval(a) - arm_height(a)));
        const int i = std::min(3, static_cast<int>(a / 22.5));
        const double lin = y[i] + (y[i + 1] - y[i]) * (a - x[i]) / 22.5;
        max_linear = std::max(max_linear, std::fabs(lin - arm_height(a)));
    }
    MESSAGE("max. Fehler kubisch ", max_cubic, " in, linear ", max_linear, " in");
    CHECK(max_linear > 0.3);  // die Geometrie ist wirklich nichtlinear genug, dass lineares Interpolieren nicht reicht
    CHECK(max_cubic <= 0.3);
}

TEST_CASE("MonotoneTable: trifft die Stuetzpunkte, kein Ueberschwingen, begrenzt statt extrapoliert") {
    std::array<double, 5> x{0, 10, 20, 30, 40}, y{0, 1, 5, 5.5, 20};
    MonotoneTable<5> t;
    REQUIRE(t.set(x, y));
    for (int i = 0; i < 5; ++i)
        CHECK(t.eval(x[i]) == doctest::Approx(y[i]));
    double prev = t.eval(0.0);
    for (double a = 0.0; a <= 40.0; a += 0.1) {
        const double v = t.eval(a);
        CHECK(v >= prev - 1e-9);  // monoton
        prev = v;
    }
    CHECK(t.eval(-5.0) == 0.0);
    CHECK(t.eval(99.0) == 20.0);
    CHECK(t.invert(-1.0) == 0.0);
    CHECK(t.invert(99.0) == 40.0);
}

TEST_CASE("MonotoneTable: Hin- und Rueckrechnung ergibt wieder den Ausgangswert") {
    std::array<double, 5> x{0, 22.5, 45, 67.5, 90}, y{};
    for (int i = 0; i < 5; ++i)
        y[i] = arm_height(x[i]);
    MonotoneTable<5> t;
    REQUIRE(t.set(x, y));
    for (double a = 0.0; a <= 90.0; a += 0.37)
        CHECK(t.invert(t.eval(a)) == doctest::Approx(a).epsilon(1e-6).scale(1.0));
}

TEST_CASE("MonotoneTable: ungueltige Tabellen werden abgelehnt") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    MonotoneTable<5> t;
    CHECK_FALSE(t.set({0, 1, 2, 3, 3}, {0, 1, 2, 3, 4}));    // x nicht streng steigend
    CHECK_FALSE(t.set({0, 1, 2, 3, 4}, {0, 1, 1, 3, 4}));    // y nicht streng steigend
    CHECK_FALSE(t.set({0, 1, 2, 4, 3}, {0, 1, 2, 3, 4}));    // x faellt
    CHECK_FALSE(t.set({0, 1, nan, 3, 4}, {0, 1, 2, 3, 4}));  // nicht endlich
    CHECK_FALSE(t.valid());
    CHECK(t.eval(1.0) == 0.0);
    REQUIRE(t.set({0, 1, 2, 3, 4}, {0, 1, 2, 3, 4}));
    CHECK(t.valid());
    CHECK_FALSE(t.set({0, 1, 2, 3, 3}, {0, 1, 2, 3, 4}));
    CHECK_FALSE(t.valid());  // eine fehlgeschlagene Eingabe macht die Tabelle ungueltig, nie halb gueltig
}
