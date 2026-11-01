#include <stdexcept>
#include <string>

#include "doctest/doctest.h"
#include "sim/json.hpp"

using frostbyte::sim::Json;
using frostbyte::sim::parse_json;

TEST_CASE("JSON: Grundtypen und null") {
    Json j = parse_json(R"({"a": null, "b": true, "c": false, "d": -12.5e1, "e": 0.25, "f": "x"})");
    CHECK(j.at("a").is_null());
    CHECK(j.at("b").as_bool());
    CHECK_FALSE(j.at("c").as_bool());
    CHECK(j.at("d").as_number() == doctest::Approx(-125.0));
    CHECK(j.at("e").as_number() == doctest::Approx(0.25));
    CHECK(j.at("f").as_string() == "x");
    CHECK(j.find("fehlt") == nullptr);
}

TEST_CASE("JSON: verschachtelte Objekte und Arrays, Reihenfolge bleibt") {
    Json j = parse_json(R"({"z": [1, [2, 3], {"k": [ ]}], "a": {"b": {"c": 7}}})");
    const Json& z = j.at("z");
    REQUIRE(z.size() == 3);
    CHECK(z[1][1].as_number() == doctest::Approx(3.0));
    CHECK(z[2].at("k").size() == 0);
    CHECK(j.at("a").at("b").at("c").as_number() == doctest::Approx(7.0));
    CHECK(j.key(0) == "z");
    CHECK(j.key(1) == "a");
}

TEST_CASE("JSON: Escapes und UTF-8-Bytes bleiben erhalten") {
    Json j = parse_json("{\"s\": \"a\\\"b\\\\c\\n\\u00e4 Zoll \xC3\xA4\"}");
    CHECK(j.at("s").as_string() == "a\"b\\c\n\xC3\xA4 Zoll \xC3\xA4");
}

TEST_CASE("JSON: Fehler werden gemeldet, nicht still ignoriert") {
    CHECK_THROWS_AS(parse_json("{\"a\": }"), std::runtime_error);
    CHECK_THROWS_AS(parse_json("[1, 2"), std::runtime_error);
    CHECK_THROWS_AS(parse_json("{} x"), std::runtime_error);
    CHECK_THROWS_AS(parse_json("{\"a\": 1}").at("b"), std::runtime_error);
    CHECK_THROWS_AS(parse_json("{\"a\": null}").at("a").as_number(), std::runtime_error);
}

TEST_CASE("JSON: Zahlen genau nach JSON-Grammatik, korrekt gerundet") {
    CHECK(parse_json("0.1").as_number() == 0.1);  // exakt, nicht Approx: gleiche Rundung auf jeder Plattform
    CHECK(parse_json("-0.5").as_number() == -0.5);
    CHECK(parse_json("1e3").as_number() == 1000.0);
    CHECK(parse_json("1E-2").as_number() == 0.01);
    CHECK(parse_json("386.1").as_number() == 386.1);
    const Json a = parse_json("[1,2]");
    REQUIRE(a.size() == 2);
    CHECK(a[1].as_number() == 2.0);
}

TEST_CASE("JSON: Zahlen ausserhalb der Grammatik oder des Wertebereichs werden abgelehnt") {
    for (const char* bad : {"0x10", "+1", ".5", "1.", "1e", "-", "1e+", "1e999", "[1.e2]"}) {
        INFO(bad);
        CHECK_THROWS_AS(parse_json(bad), std::runtime_error);
    }
}
