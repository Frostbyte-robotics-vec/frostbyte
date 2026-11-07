#include <cstring>

#include "doctest/doctest.h"
#include "util/build_info.hpp"

using namespace frostbyte;

TEST_CASE("BuildInfo: sauberer Stand zeigt Version, Commit und Datum") {
    char buf[BuildInfoText::cols + 1];
    format_build_info(BuildInfo{"1.2.0", "abc1234", false, "Nov  7 2026"}, buf, sizeof buf);
    CHECK(std::strcmp(buf, "v1.2.0 abc1234 Nov  7 2026") == 0);
}

TEST_CASE("BuildInfo: ungesicherte Aenderungen werden als dirty markiert") {
    char buf[BuildInfoText::cols + 1];
    format_build_info(BuildInfo{"1.2.0", "abc1234", true, ""}, buf, sizeof buf);
    CHECK(std::strcmp(buf, "v1.2.0 abc1234 dirty") == 0);
}

TEST_CASE("BuildInfo: ohne git (Commit leer) steht ein Strich, nie ein leeres Feld") {
    char buf[BuildInfoText::cols + 1];
    format_build_info(BuildInfo{"1.2.0", "", false, ""}, buf, sizeof buf);
    CHECK(std::strcmp(buf, "v1.2.0 -") == 0);
    format_build_info(BuildInfo{"1.2.0", nullptr, true, nullptr}, buf, sizeof buf);
    CHECK(std::strcmp(buf, "v1.2.0 - dirty") == 0);
}

TEST_CASE("BuildInfo: zu langer Text wird abgeschnitten, aber dirty bleibt sichtbar") {
    char buf[BuildInfoText::cols + 1];
    format_build_info(BuildInfo{"10.20.30-quali-feb-final", "0123456789abcdef", true, "Nov  7 2026"}, buf, sizeof buf);
    CHECK(std::strlen(buf) <= BuildInfoText::cols);
    CHECK(std::strstr(buf, "dirty") != nullptr);
}

TEST_CASE("BuildInfo: kleiner Puffer wird nie ueberschrieben und bleibt terminiert") {
    char buf[8];
    std::memset(buf, 'x', sizeof buf);
    format_build_info(BuildInfo{"1.2.0", "abc1234", true, "Nov  7 2026"}, buf, 5);
    CHECK(std::strlen(buf) <= 4);
    CHECK(buf[5] == 'x');
    format_build_info(BuildInfo{"1.2.0", "abc1234", false, ""}, buf, 0);  // darf nichts tun
    CHECK(buf[5] == 'x');
}
