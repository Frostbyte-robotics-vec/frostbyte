#include <cmath>
#include <string>

#include "core/log_format.hpp"
#include "doctest/doctest.h"

using namespace frostbyte;

TEST_CASE("Log v1: Spaltenzeile exakt wie docs/log_format.md") {
    CHECK(std::string(log::columns) ==
          "t_s,robot,x_in,y_in,h_deg,gx_in,gy_in,gh_deg,g_err_in,tx_in,ty_in,th_deg,lift_deg,arm_deg,claw_in,"
          "lift_set_deg,arm_set_deg,cycle,route_step,has_cup,has_pin,pin_yellow_up,score,cpu_ms,batt_v,temp_max_c,event");
}

TEST_CASE("Log v1: Kopfzeile, Seed fehlt -> Strich") {
    char buf[128];
    log::format_header(buf, sizeof buf, "stacker", "sim", 42, "abc123");
    CHECK(std::string(buf) == "# frostbyte-log v1 robot=stacker source=sim seed=42 build=abc123");
    log::format_header(buf, sizeof buf, "runner", "robot", log::no_seed, "abc123");
    CHECK(std::string(buf) == "# frostbyte-log v1 robot=runner source=robot seed=- build=abc123");
}

TEST_CASE("Log v1: leere Zeile hat nur Zeit und Roboter, Rest leer") {
    char buf[512];
    log::Row r;
    r.t_s = 1.24;
    r.robot = "runner";
    log::format_row(buf, sizeof buf, r);
    CHECK(std::string(buf) == "1.240,runner,,,,,,,,,,,,,,,,,,,,,,,,,");
}

TEST_CASE("Log v1: Zahlen mit fester Präzision, keine negative Null, Ganzzahl-Flags") {
    char buf[512];
    log::Row r;
    r.t_s = 0.0;
    r.robot = "stacker";
    r.x_in = -12.3456;
    r.y_in = -0.0001;
    r.h_deg = 90.0;
    r.cycle = "BEREIT";
    r.has_cup = 1;
    r.score = 0;
    r.event = "BLOCKED;RULE:SG6";
    log::format_row(buf, sizeof buf, r);
    CHECK(std::string(buf) == "0.000,stacker,-12.346,0.000,90.000,,,,,,,,,,,,,BEREIT,,1,,,0,,,,BLOCKED;RULE:SG6");
}
