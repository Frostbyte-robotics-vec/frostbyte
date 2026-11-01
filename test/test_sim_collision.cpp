#include <cmath>

#include "doctest/doctest.h"
#include "sim/collision.hpp"

using namespace frostbyte::sim;

namespace {
// Testmaße, unabhängig von sim_params.json.
Box box(double x, double y, double h, double len = 10.0, double wid = 10.0) { return Box{x, y, len, wid, h}; }
}  // namespace

TEST_CASE("Kollision: getrennte Boxen berühren sich nicht") {
    CHECK_FALSE(penetration(box(0, 0, 0), box(11, 0, 0)).has_value());
    CHECK_FALSE(penetration(box(0, 0, 45), box(0, 20, 45)).has_value());
}

TEST_CASE("Kollision: nur Berührung der Kanten zählt nicht") {
    CHECK_FALSE(penetration(box(0, 0, 0), box(10, 0, 0)).has_value());
}

TEST_CASE("Kollision: Überlappung entlang x, Verschiebung trennt genau") {
    const auto mtv = penetration(box(0, 0, 0), box(8, 0, 0));
    REQUIRE(mtv.has_value());
    // A muss um 2 Zoll von B weg (nach -x).
    CHECK(mtv->x_in == doctest::Approx(-2.0));
    CHECK(mtv->y_in == doctest::Approx(0.0).epsilon(1e-9));
}

TEST_CASE("Kollision: gedrehte Box (45 Grad) überlappt nur bei echtem Kontakt") {
    // Gedrehtes Quadrat 10x10: Halbdiagonale 7,07. Ecke ragt bis x = 7,07.
    CHECK(penetration(box(0, 0, 0), box(11.5, 0, 45)).has_value());
    CHECK_FALSE(penetration(box(0, 0, 0), box(12.5, 0, 45)).has_value());
    // Auf der Diagonalen ist der Abstand der Bounding-Boxes kleiner als der echte: kein Kontakt.
    CHECK_FALSE(penetration(box(0, 0, 0), box(11.0, 11.0, 45)).has_value());
}

TEST_CASE("Kollision: Verschiebung ist symmetrisch (A->B = -(B->A))") {
    const auto ab = penetration(box(0, 0, 10), box(7, 3, 30));
    const auto ba = penetration(box(7, 3, 30), box(0, 0, 10));
    REQUIRE(ab.has_value());
    REQUIRE(ba.has_value());
    CHECK(ab->x_in == doctest::Approx(-ba->x_in));
    CHECK(ab->y_in == doctest::Approx(-ba->y_in));
}

TEST_CASE("Feldrand: Box innerhalb, Verschiebung ist null") {
    const auto d = push_inside_field(box(0, 0, 0), 144.0);
    CHECK(d.x_in == 0.0);
    CHECK(d.y_in == 0.0);
}

TEST_CASE("Feldrand: ungedrehte Box ragt über die Wand, wird zurückgeschoben") {
    // Feld 144: Wand bei x = 72. Box 10 breit bei x = 70 reicht bis 75.
    const auto d = push_inside_field(box(70, 0, 0), 144.0);
    CHECK(d.x_in == doctest::Approx(-3.0));
    CHECK(d.y_in == doctest::Approx(0.0));
}

TEST_CASE("Feldrand: gedrehte Box zählt mit der Ecke") {
    // 45 Grad: Halbdiagonale 7,07; bei x = 66 reicht die Ecke bis 73,07.
    const auto d = push_inside_field(box(66, 0, 45), 144.0);
    CHECK(d.x_in == doctest::Approx(-(66 + 7.0710678 - 72.0)).epsilon(1e-6));
}

TEST_CASE("Feldrand: Ecke in der Ecke wird auf beiden Achsen zurückgeschoben") {
    const auto d = push_inside_field(box(70, -70, 0), 144.0);
    CHECK(d.x_in == doctest::Approx(-3.0));
    CHECK(d.y_in == doctest::Approx(3.0));
}

TEST_CASE("Feldelement (Kreis): Kontakt und Abstand") {
    const Circle c{20.0, 0.0, 3.0};
    CHECK_FALSE(penetration(box(0, 0, 0), c).has_value());   // Box bis x = 5, Kreis ab 17
    const auto mtv = penetration(box(14, 0, 0), c);          // Box bis 19, Kreis ab 17 -> 2 Zoll
    REQUIRE(mtv.has_value());
    CHECK(mtv->x_in == doctest::Approx(-2.0));
    CHECK(mtv->y_in == doctest::Approx(0.0).epsilon(1e-9));
}

TEST_CASE("Feldelement (Kreis): Ecke der Box trifft den Kreis, Kante nicht") {
    const Circle c{8.0, 8.0, 3.0};
    // Box bis (5,5): Abstand der Ecke zum Mittelpunkt 4,24 > 3: kein Kontakt, obwohl die Bounding-Boxes sich überlappen.
    CHECK_FALSE(penetration(box(0, 0, 0), c).has_value());
    CHECK(penetration(box(2, 2, 0), c).has_value());
}
