#include "doctest/doctest.h"
#include "util/units.hpp"

TEST_CASE("Grad und Bogenmaß") {
    CHECK(frostbyte::deg_to_rad(180.0) == doctest::Approx(3.14159265358979));
    CHECK(frostbyte::rad_to_deg(3.14159265358979) == doctest::Approx(180.0));
}

TEST_CASE("wrap_deg bringt Winkel nach (-180, 180]") {
    CHECK(frostbyte::wrap_deg(0.0) == doctest::Approx(0.0));
    CHECK(frostbyte::wrap_deg(190.0) == doctest::Approx(-170.0));
    CHECK(frostbyte::wrap_deg(-190.0) == doctest::Approx(170.0));
    CHECK(frostbyte::wrap_deg(180.0) == doctest::Approx(180.0));
    CHECK(frostbyte::wrap_deg(-180.0) == doctest::Approx(180.0));
    CHECK(frostbyte::wrap_deg(720.0 + 45.0) == doctest::Approx(45.0));
}
