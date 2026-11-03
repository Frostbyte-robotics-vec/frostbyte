#include <sstream>
#include <stdexcept>
#include <string>

#include "doctest/doctest.h"
#include "sim/input_script.hpp"

using namespace frostbyte::sim;

namespace {

InputScript parse(const std::string& text, int steuer_ms = 10) {
    std::istringstream in(text);
    return parse_input_script(in, steuer_ms);
}

}  // namespace

TEST_CASE("Eingabeverlauf: Spannung gilt ab t_ms bis zur nächsten Zeile, davor 0 V") {
    const InputScript s = parse(
        "# Kommentar\n"
        "t_ms,robot,left_V,right_V\n"
        "100,stacker,6,6\n"
        "300,stacker,-4,4\n"
        "\n"
        "0,runner,2,3\n");
    const auto st = input_open_loop(s.stacker);
    CHECK(st(0).first == 0.0);
    CHECK(st(99).first == 0.0);
    CHECK(st(100) == std::make_pair(6.0, 6.0));
    CHECK(st(299) == std::make_pair(6.0, 6.0));
    CHECK(st(300) == std::make_pair(-4.0, 4.0));
    CHECK(st(59999) == std::make_pair(-4.0, 4.0));  // letzte Zeile gilt weiter
    const auto ru = input_open_loop(s.runner);
    CHECK(ru(0) == std::make_pair(2.0, 3.0));
}

TEST_CASE("Eingabeverlauf: ohne Zeilen eines Roboters bleibt dessen Liste leer") {
    const InputScript s = parse("t_ms,robot,left_V,right_V\n0,stacker,1,1\n");
    CHECK(s.stacker.size() == 1);
    CHECK(s.runner.empty());
}

TEST_CASE("Eingabeverlauf: Fehler werden mit Zeilennummer gemeldet") {
    CHECK_THROWS_AS(parse("0,stacker,1,1\n"), std::runtime_error);  // Kopfzeile fehlt
    CHECK_THROWS_AS(parse("t_ms,robot,left_V,right_V\n5,stacker,1,1\n"),
                    std::runtime_error);  // nicht auf dem Steuertakt
    CHECK_THROWS_AS(parse("t_ms,robot,left_V,right_V\n10,bagger,1,1\n"), std::runtime_error);   // unbekannter Roboter
    CHECK_THROWS_AS(parse("t_ms,robot,left_V,right_V\n10,stacker,1\n"), std::runtime_error);    // zu wenige Spalten
    CHECK_THROWS_AS(parse("t_ms,robot,left_V,right_V\n10,stacker,1,x\n"), std::runtime_error);  // keine Zahl
    CHECK_THROWS_AS(parse("t_ms,robot,left_V,right_V\n20,stacker,1,1\n10,stacker,1,1\n"),
                    std::runtime_error);                                                         // rückwärts
    CHECK_THROWS_AS(parse("t_ms,robot,left_V,right_V\n-10,stacker,1,1\n"), std::runtime_error);  // negative Zeit
    try {
        parse("t_ms,robot,left_V,right_V\n10,stacker,1,1\n15,stacker,1,1\n");
        FAIL("hätte werfen müssen");
    } catch (const std::runtime_error& e) {
        CHECK(std::string(e.what()).find("Zeile 3") != std::string::npos);
    }
}
