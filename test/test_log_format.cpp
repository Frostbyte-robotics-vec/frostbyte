#include <cmath>
#include <string>

#include "core/log_format.hpp"
#include "doctest/doctest.h"

using namespace frostbyte;

TEST_CASE("Log v1: Spaltenzeile exakt wie docs/log_format.md") {
    CHECK(std::string(logfmt::columns) ==
          "t_s,robot,x_in,y_in,h_deg,gx_in,gy_in,gh_deg,g_err_in,tx_in,ty_in,th_deg,lift_deg,arm_deg,claw_in,"
          "lift_set_deg,arm_set_deg,cycle,route_step,has_cup,has_pin,pin_yellow_up,score,cpu_ms,batt_v,temp_max_c,event");
}

TEST_CASE("Log v1: Kopfzeile, Seed fehlt -> Strich") {
    char buf[128];
    logfmt::format_header(buf, sizeof buf, "stacker", "sim", 42, "abc123");
    CHECK(std::string(buf) == "# frostbyte-log v1 robot=stacker source=sim seed=42 build=abc123");
    logfmt::format_header(buf, sizeof buf, "runner", "robot", logfmt::no_seed, "abc123");
    CHECK(std::string(buf) == "# frostbyte-log v1 robot=runner source=robot seed=- build=abc123");
}

TEST_CASE("Log v1: leere Zeile hat nur Zeit und Roboter, Rest leer") {
    char buf[512];
    logfmt::Row r;
    r.t_s = 1.24;
    r.robot = "runner";
    logfmt::format_row(buf, sizeof buf, r);
    CHECK(std::string(buf) == "1.240,runner,,,,,,,,,,,,,,,,,,,,,,,,,");
}

TEST_CASE("Log v1: Zahlen mit fester Präzision, keine negative Null, Ganzzahl-Flags") {
    char buf[512];
    logfmt::Row r;
    r.t_s = 0.0;
    r.robot = "stacker";
    r.x_in = -12.3456;
    r.y_in = -0.0001;
    r.h_deg = 90.0;
    r.cycle = "BEREIT";
    r.has_cup = 1;
    r.score = 0;
    r.event = "BLOCKED;RULE:SG6";
    logfmt::format_row(buf, sizeof buf, r);
    CHECK(std::string(buf) == "0.000,stacker,-12.346,0.000,90.000,,,,,,,,,,,,,BEREIT,,1,,,0,,,,BLOCKED;RULE:SG6");
}

TEST_CASE("Log v1: Abschneiden wird ueber den Rueckgabewert gemeldet, Puffer bleibt abgeschlossen") {
    char buf[16];
    logfmt::Row r;
    r.t_s = 12.5;
    r.robot = "stacker";
    r.event = "BLOCKED;RULE:SG6";
    const int need = logfmt::format_row(buf, sizeof buf, r);
    CHECK(need >= static_cast<int>(sizeof buf));
    CHECK(std::string(buf) == "12.500,stacker,");
    char big[512];
    CHECK(logfmt::format_row(big, sizeof big, r) == need);

    char head[32];
    CHECK(logfmt::format_header(head, sizeof head, "stacker", "sim", 1, "sehr-langer-build-name") >=
          static_cast<int>(sizeof head));
    CHECK(std::string(head).size() == sizeof head - 1);
}

TEST_CASE("Log v1: Text mit Komma, Anfuehrungszeichen oder Zeilenende verschiebt keine Spalten") {
    char buf[512];
    logfmt::Row r;
    r.t_s = 0.0;
    r.robot = "runner";
    r.cycle = "A,B";
    r.route_step = "12:cycle \"X\"";
    r.event = "E\r\nF";
    logfmt::format_row(buf, sizeof buf, r);
    CHECK(std::string(buf) == "0.000,runner,,,,,,,,,,,,,,,,A_B,12:cycle _X_,,,,,,,,E__F");
}

TEST_CASE("Log v1: Ersetzen bleibt innerhalb des Puffers, auch genau an der Abschneidegrenze") {
    logfmt::Row r;
    r.t_s = 0.0;
    r.robot = "ab,cd";
    char buf9[9];
    logfmt::format_row(buf9, sizeof buf9, r);
    CHECK(std::string(buf9) == "0.000,ab");
    char buf10[10];
    logfmt::format_row(buf10, sizeof buf10, r);
    CHECK(std::string(buf10) == "0.000,ab_");
}
