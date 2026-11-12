#include <cmath>
#include <fstream>
#include <sstream>
#include <string>

#include "doctest/doctest.h"
#include "gen/test_square_route.hpp"
#include "sim/route_parser.hpp"

using namespace frostbyte;
using namespace frostbyte::sim;

namespace {

// Eigene Namen, unabhängig von den Werten in field.json.
RouteNames names() {
    RouteNames n;
    n.goals = {"G_A", "G_B", "MIDFIELD"};
    n.loaders = {"L_A"};
    n.toggles = {"T_A"};
    n.poses = {"P_START", "P_MID"};
    return n;
}

const std::string kHead = "start P_START\n";
const std::string kTail = "end hold\n";

RouteResult parse(const std::string& text) { return parse_route(text, names()); }

struct Bad {
    const char* text;
    int line;
    const char* part;  // Teilstring der Meldung
};

}  // namespace

TEST_CASE("route: gültige Route wird vollständig gelesen, Blöcke zählen ihre Befehle") {
    const auto r = parse(
        "# Kommentar\n"
        "target_p10 90\n"
        "start P_START   # am Rand\n"
        "cycle source=preload goal=G_A level=auto\n"
        "repeat 3 {\n"
        "  goto L_A tol=1.5 timeout=3.0 on_timeout=retry\n"
        "  intake until=unit timeout=2.5\n"
        "  parallel {\n"
        "    cycle goal=G_B level=2 timeout=6\n"
        "    wait 0.5\n"
        "  }\n"
        "}\n"
        "at 52s goto P_MID timeout=6\n"
        "end hold\n");
    REQUIRE_MESSAGE(r.ok(), r.error);
    REQUIRE(r.cmds.size() == 12);
    CHECK(r.cmds[4].op == RouteOp::goto_name);
    CHECK(r.cmds[4].on_timeout == RouteOnTimeout::retry);
    CHECK(r.cmds[0].op == RouteOp::target_p10);
    CHECK(r.cmds[0].value == doctest::Approx(90));
    CHECK(r.cmds[1].op == RouteOp::start);
    CHECK(std::string(r.cmds[1].name) == "P_START");
    CHECK(r.cmds[2].level == kRouteLevelAuto);
    CHECK(r.cmds[3].op == RouteOp::repeat);
    CHECK(r.cmds[3].value == doctest::Approx(3));
    CHECK(r.cmds[3].body_len == 5);  // goto, intake, parallel, cycle, wait (verschachtelte mitgezählt)
    CHECK(r.cmds[6].op == RouteOp::parallel);
    CHECK(r.cmds[6].body_len == 2);
    CHECK(r.cmds[7].level == 2);
    CHECK(r.cmds[8].op == RouteOp::wait);
    CHECK(r.cmds[9].op == RouteOp::at);
    CHECK(r.cmds[9].value == doctest::Approx(52));
    CHECK(r.cmds[10].op == RouteOp::goto_name);
}

TEST_CASE("route: Felder von goto, cycle, parallel und at") {
    const auto r = parse(kHead +
                         "goto L_A tol=1.5 timeout=3 on_timeout=abort\n"
                         "goto pose x=1 y=-2 h=90 timeout=4\n"
                         "turn 270\n"
                         "cycle goal=G_B level=5 source=quiver\n"
                         "toggle T_A timeout=2\n"
                         "signal ready\n"
                         "wait_for stacker.ready timeout=5\n"
                         "at 40s turn 10\n" +
                         kTail);
    REQUIRE_MESSAGE(r.ok(), r.error);
    const auto& c = r.cmds;
    CHECK(c[1].op == RouteOp::goto_name);
    CHECK(c[1].tol_in == doctest::Approx(1.5));
    CHECK(c[1].timeout_s == doctest::Approx(3));
    CHECK(c[1].on_timeout == RouteOnTimeout::abort);
    CHECK(c[2].op == RouteOp::goto_pose);
    CHECK(c[2].x_in == doctest::Approx(1));
    CHECK(c[2].y_in == doctest::Approx(-2));
    CHECK(c[2].h_deg == doctest::Approx(90));
    CHECK(c[3].h_deg == doctest::Approx(270));
    CHECK(c[4].level == 5);
    CHECK(c[4].source == RouteSource::quiver);
    CHECK(c[7].robot == RouteRobot::stacker);
    CHECK(std::string(c[7].name) == "ready");
    CHECK(c[8].op == RouteOp::at);
    CHECK(c[8].body_len == 1);
    CHECK(c[8].value == doctest::Approx(40));
    CHECK(c[9].op == RouteOp::turn);
    CHECK(c.back().op == RouteOp::end_hold);
}

TEST_CASE("route: start mit Pose und verschachtelte Blöcke") {
    const auto r = parse(
        "start pose x=0 y=0 h=0\n"
        "parallel {\n"
        "  repeat 2 {\n"
        "    wait 1\n"
        "  }\n"
        "  wait 2\n"
        "}\n"
        "end hold\n");
    REQUIRE_MESSAGE(r.ok(), r.error);
    CHECK(r.cmds[0].has_pose);
    CHECK(r.cmds[1].op == RouteOp::parallel);
    CHECK(r.cmds[1].body_len == 3);  // repeat, wait, wait
    CHECK(r.cmds[2].op == RouteOp::repeat);
    CHECK(r.cmds[2].body_len == 1);
}

TEST_CASE("route: fehlerhafte Routen melden Zeilennummer und Grund") {
    const Bad bad[] = {
        {"start P_START\nfly 3\nend hold\n", 2, "unbekannte Anweisung 'fly'"},
        {"start P_START\ngoto L_A speed=3\nend hold\n", 2, "unbekannter Schlüssel 'speed'"},
        {"start P_START\ngoto L_A timeout=3 timeout=4\nend hold\n", 2, "doppelt"},
        {"start P_START\ncycle goal=G_A\nend hold\n", 2, "level"},
        {"start P_START\ncycle level=1\nend hold\n", 2, "goal"},
        {"start P_START\ngoto L_A timeout=abc\nend hold\n", 2, "keine Zahl"},
        {"start P_START\ngoto L_A timeout=-1\nend hold\n", 2, "größer als 0"},
        {"start P_START\ncycle goal=G_A level=7\nend hold\n", 2, "level"},
        {"start P_START\ncycle goal=G_A level=-1\nend hold\n", 2, "level"},
        {"start P_START\ncycle goal=G_A level=1.5\nend hold\n", 2, "level"},
        {"start P_START\ngoto NIRGENDS\nend hold\n", 2, "unbekannter Name 'NIRGENDS'"},
        {"start P_START\ncycle goal=L_A level=1\nend hold\n", 2, "Loader"},
        {"start G_A\nend hold\n", 1, "Pose"},
        {"start P_START\ntoggle G_A\nend hold\n", 2, "Goal"},
        {"start P_START\nwait 1 on_timeout=skip\nend hold\n", 2, "on_timeout"},
        {"start P_START\nsignal go on_timeout=skip\nend hold\n", 2, "on_timeout"},
        {"start P_START\ngoto L_A on_timeout=egal\nend hold\n", 2, "on_timeout"},
        {"start P_START\nrepeat 2 {\n  wait 1\nend hold\n", 4, "obersten Ebene"},
        {"start P_START\nrepeat 2 {\n  wait 1\n", 2, "nicht geschlossen"},
        {"start P_START\nwait 1\n}\nend hold\n", 3, "'}' ohne"},
        {"start P_START\nat 52 turn 0\nend hold\n", 2, "52s"},
        {"start P_START\nwait_for kamel.go\nend hold\n", 2, "Roboter 'kamel'"},
        {"start P_START\nwait_for go\nend hold\n", 2, "<robot>.<name>"},
        {"start P_START\nend hold\nwait 1\n", 3, "nach 'end hold'"},
        {"start P_START\nwait 1\n", 2, "end hold"},
        {"wait 1\nend hold\n", 1, "start"},
        {"start P_START\nstart P_MID\nend hold\n", 2, "start"},
        {"start P_START\ntarget_p10 5\nend hold\n", 2, "target_p10"},
        {"target_p10 5\ntarget_p10 6\nstart P_START\nend hold\n", 2, "target_p10"},
        {"start P_START\nrepeat 0 {\n wait 1\n}\nend hold\n", 2, "repeat"},
        {"start P_START\nrepeat 2.5 {\n wait 1\n}\nend hold\n", 2, "repeat"},
        {"start P_START\nrepeat 2 {\n  end hold\n}\nend hold\n", 3, "obersten Ebene"},
        {"start P_START\nrepeat 2 {\n  at 5s wait 1\n}\nend hold\n", 3, "obersten Ebene"},
        {"start P_START\nat 5s at 6s wait 1\nend hold\n", 2, "at"},
        {"start P_START\nat 5s parallel {\n wait 1\n}\nend hold\n", 2, "Block"},
        {"start P_START\nend\n", 2, "end hold"},
        {"start P_START\nend hold extra\n", 2, "end hold"},
        {"start pose x=0 y=0\nend hold\n", 1, "h"},
        {"start P_START\ngoto pose x=0 y=0 h=0 tol=1\nend hold\n", 2, "tol"},
        {"start P_START\nintake\nend hold\n", 2, "until=unit"},
        {"start P_START\nintake until=cup\nend hold\n", 2, "until=unit"},
        {"start P_START\ncycle goal=G_A level=1 source=heap\nend hold\n", 2, "source"},
        {"start P_START\nturn\nend hold\n", 2, "turn"},
        {"start P_START\nturn 90 91\nend hold\n", 2, "turn"},
        {"start P_START\nwait\nend hold\n", 2, "wait"},
        {"start P_START\nparallel\nwait 1\nend hold\n", 2, "{"},
        {"start P_START\ngoto L_A tol\nend hold\n", 2, "Schlüssel 'tol' ohne Wert"},
        {"start P_START\nsignal ein_viel_zu_langer_ereignisname_der_nicht_mehr_passt\nend hold\n", 2, "zu lang"},
        {"start P_START\nsignal GROSS\nend hold\n", 2, "Ereignisname"},
        {"", 1, "start"},
    };
    for (const Bad& b : bad) {
        INFO("Route:\n" << std::string(b.text));
        const auto r = parse(b.text);
        REQUIRE_FALSE(r.ok());
        CHECK(r.error_line == b.line);
        CHECK(r.error.find("Zeile " + std::to_string(b.line) + ":") == 0);
        CHECK_MESSAGE(r.error.find(b.part) != std::string::npos, "Meldung war: ", r.error);
    }
}

TEST_CASE("route: nicht geschlossener Block meldet die Zeile der öffnenden Klammer") {
    const auto r = parse(kHead + "wait 1\nparallel {\n wait 1\n");
    REQUIRE_FALSE(r.ok());
    CHECK(r.error_line == 3);
}

TEST_CASE("route: Windows-Zeilenenden und Tabs") {
    const auto r = parse("start\tP_START\r\nwait 1\r\nend hold\r\n");
    CHECK_MESSAGE(r.ok(), r.error);
}

// Bewusst nur Syntax: Zeiten und Ziele der Routen (z. B. target_p10) sind Entwurf und kein Testgegenstand (F-13).
TEST_CASE(
    "route: alle Routen in routes/ lesen sich (nur Syntax und Namen; Zeiten und Ziele sind Entwurf und werden nicht "
    "geprüft)") {
    const RouteNames n = route_names_from_field(std::string(FB_ROOT_DIR) + "/field/field.json");
    CHECK(n.goals.size() == 9);
    CHECK(n.loaders.size() == 4);
    CHECK(n.toggles.size() == 4);
    CHECK(n.poses.size() == 4);
    for (const char* f : {"skills_stacker.route", "skills_runner.route", "test_square.route"}) {
        std::ifstream in(std::string(FB_ROOT_DIR) + "/routes/" + f, std::ios::binary);
        REQUIRE(in);
        std::stringstream ss;
        ss << in.rdbuf();
        const auto r = parse_route(ss.str(), n);
        INFO(f);
        CHECK_MESSAGE(r.ok(), r.error);
    }
}

TEST_CASE("route: C++-Ausgabe enthält jeden Befehl und ist bei gleicher Eingabe gleich") {
    const auto r = parse(kHead + "goto L_A tol=1.5 timeout=3\ncycle goal=G_A level=auto\n" + kTail);
    REQUIRE(r.ok());
    const std::string a = route_to_cpp(r.cmds, "kRouteTest");
    CHECK(a == route_to_cpp(r.cmds, "kRouteTest"));
    CHECK(a.find("kRouteTest") != std::string::npos);
    CHECK(a.find("RouteOp::goto_name") != std::string::npos);
    CHECK(a.find("\"L_A\"") != std::string::npos);
    CHECK(a.find("RouteOp::end_hold") != std::string::npos);
}

// Der von route2cpp erzeugte Header (CMake erzeugt ihn aus routes/test_square.route) stimmt Feld für Feld mit dem Parser überein.
TEST_CASE("route: erzeugter Header entspricht dem Parser-Ergebnis") {
    const RouteNames n = route_names_from_field(std::string(FB_ROOT_DIR) + "/field/field.json");
    std::ifstream in(std::string(FB_ROOT_DIR) + "/routes/test_square.route", std::ios::binary);
    REQUIRE(in);
    std::stringstream ss;
    ss << in.rdbuf();
    const auto r = parse_route(ss.str(), n);
    REQUIRE_MESSAGE(r.ok(), r.error);
    REQUIRE(routes::kRouteTestSquareLen == static_cast<int>(r.cmds.size()));
    for (int i = 0; i < routes::kRouteTestSquareLen; ++i) {
        INFO("Befehl " << i);
        const RouteCmd& g = routes::kRouteTestSquare[i];
        const RouteCmd& p = r.cmds[static_cast<std::size_t>(i)];
        CHECK(g.op == p.op);
        CHECK(g.line == p.line);
        CHECK(std::string(g.name) == std::string(p.name));
        CHECK(g.has_pose == p.has_pose);
        CHECK(g.x_in == p.x_in);
        CHECK(g.y_in == p.y_in);
        CHECK(g.h_deg == p.h_deg);
        CHECK(g.value == p.value);
        CHECK(g.tol_in == p.tol_in);
        CHECK(g.timeout_s == p.timeout_s);
        CHECK(g.level == p.level);
        CHECK(g.body_len == p.body_len);
        CHECK(g.source == p.source);
        CHECK(g.on_timeout == p.on_timeout);
        CHECK(g.robot == p.robot);
    }
    CHECK(routes::kRouteTestSquare[0].has_pose);  // start pose x=0 y=0 h=0
    CHECK(routes::kRouteTestSquare[1].x_in == doctest::Approx(48));
    CHECK(routes::kRouteTestSquare[1].h_deg == doctest::Approx(90));
}
