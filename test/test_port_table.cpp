#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "config/runner.hpp"
#include "config/stacker.hpp"
#include "doctest/doctest.h"
#include "util/port_table.hpp"

using namespace frostbyte;

namespace {
const PortExpect kOk[] = {
    {1, DeviceKind::motor, "A", "vor"},
    {-2, DeviceKind::motor, "B", "vor"},
    {unassigned_port, DeviceKind::imu, "C", nullptr},
    {unassigned_port, DeviceKind::gps, "D", nullptr},  // mehrere offene Ports sind erlaubt
};

std::string read_file(const std::string& path) {
    std::ifstream in(path);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::vector<std::string> split_cells(const std::string& line) {
    std::vector<std::string> cells;
    std::string cur;
    for (std::size_t i = 1; i < line.size(); ++i) {  // erstes '|' überspringen
        if (line[i] == '|') {
            std::size_t a = cur.find_first_not_of(' ');
            std::size_t b = cur.find_last_not_of(' ');
            cells.push_back(a == std::string::npos ? "" : cur.substr(a, b - a + 1));
            cur.clear();
        } else {
            cur += line[i];
        }
    }
    return cells;
}

// Zeilen aus docs/ports.md: | Roboter | Gerät | Art | Port | Drehrichtung | Getriebe | Quelle |
std::vector<std::vector<std::string>> doc_rows(const std::string& robot) {
    std::vector<std::vector<std::string>> rows;
    std::istringstream in(read_file(std::string(FB_ROOT_DIR) + "/docs/ports.md"));
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("| " + robot + " |", 0) == 0)
            rows.push_back(split_cells(line));
    }
    return rows;
}

std::string port_text(int port) {
    return port == unassigned_port ? "TODO(HW)" : std::to_string(port < 0 ? -port : port);
}

void check_robot(const char* robot, const PortExpect* table, int count) {
    CAPTURE(robot);
    REQUIRE(count > 0);
    CHECK(port_table_check(table, count) == PortIssue::ok);
    const auto rows = doc_rows(robot);
    CHECK(static_cast<int>(rows.size()) == count);  // jede Zeile der Tabelle hat genau einen Config-Eintrag
    for (int i = 0; i < count; ++i) {
        CAPTURE(table[i].name);
        bool found = false;
        for (const auto& r : rows) {
            REQUIRE(r.size() >= 7);
            if (r[1] != table[i].name)
                continue;
            found = true;
            CHECK(r[3] == port_text(table[i].port));  // Tabelle und config/ stimmen überein
        }
        CHECK(found);
    }
}
}  // namespace

TEST_CASE("port_table_check: gueltige Tabelle, offene Ports erlaubt") {
    CHECK(port_table_check(kOk, 4) == PortIssue::ok);
    CHECK(unassigned_count(kOk, 4) == 2);
}

TEST_CASE("port_table_check: doppelter Port, auch mit umgekehrtem Vorzeichen") {
    const PortExpect dup[] = {{3, DeviceKind::motor, "A", nullptr}, {-3, DeviceKind::motor, "B", nullptr}};
    CHECK(port_table_check(dup, 2) == PortIssue::duplicate);
}

TEST_CASE("port_table_check: Port ausserhalb 1..21") {
    const PortExpect hi[] = {{22, DeviceKind::motor, "A", nullptr}};
    const PortExpect lo[] = {{-22, DeviceKind::motor, "A", nullptr}};
    CHECK(port_table_check(hi, 1) == PortIssue::out_of_range);
    CHECK(port_table_check(lo, 1) == PortIssue::out_of_range);
    const PortExpect edge[] = {{1, DeviceKind::motor, "A", nullptr}, {21, DeviceKind::motor, "B", nullptr}};
    CHECK(port_table_check(edge, 2) == PortIssue::ok);
}

TEST_CASE("port_table_check: leere Tabelle ist ok") { CHECK(port_table_check(nullptr, 0) == PortIssue::ok); }

TEST_CASE("port_valid und checked_port") {
    CHECK_FALSE(port_valid(unassigned_port));
    CHECK_FALSE(port_valid(22));
    CHECK(port_valid(1));
    CHECK(port_valid(21));
    static_assert(checked_port(5) == 5);  // zur Compile-Zeit auswertbar, solange der Port gueltig ist
    CHECK(checked_port(21) == 21);
    // checked_port(unassigned_port) bricht zur Compile-Zeit ab (kein Konstantenausdruck) bzw. zur Laufzeit mit abort().
}

TEST_CASE("config: Port-Tabelle Stacker stimmt mit docs/ports.md ueberein") {
    check_robot("stacker", config::stacker::port_expect, config::stacker::port_expect_count);
}

TEST_CASE("config: Port-Tabelle Runner stimmt mit docs/ports.md ueberein") {
    check_robot("runner", config::runner::port_expect, config::runner::port_expect_count);
}

TEST_CASE("Ports stehen nur in config/: kein Port-Literal bei PROS-Geraeten in src/, lib/, hal/") {
    // Treffer wie pros::Motor m(3) oder pros::Imu imu{4}: Zahl direkt im Konstruktor.
    const std::regex literal(R"(pros::(Motor|Rotation|Imu|Gps|Distance|Optical|Vision)\s*\w*\s*[({]\s*-?\d)");
    int hits = 0;
    for (const char* dir : {"src", "lib", "hal"}) {
        const std::filesystem::path root = std::filesystem::path(FB_ROOT_DIR) / dir;
        if (!std::filesystem::exists(root))
            continue;
        for (const auto& e : std::filesystem::recursive_directory_iterator(root)) {
            const auto ext = e.path().extension();
            if (!e.is_regular_file() || (ext != ".cpp" && ext != ".hpp"))
                continue;
            if (std::regex_search(read_file(e.path().string()), literal)) {
                ++hits;
                MESSAGE("Port-Literal in " << e.path().string());
            }
        }
    }
    CHECK(hits == 0);
}
