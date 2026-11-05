#include <cmath>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "doctest/doctest.h"
#include "sim/world.hpp"

using namespace frostbyte::sim;

namespace {

const std::string fixtures = FB_TEST_FIXTURES;

// Spalten nach docs/log_format.md.
constexpr int col_x = 2, col_y = 3, col_h = 4, col_gx = 5, col_gy = 6, col_tx = 9, col_ty = 10;

SimConfig one_robot(const std::string& profile) {
    return load_config(fixtures + "/sim_params_min.json", fixtures + "/field_min.json", fixtures + "/empty.route",
                       fixtures + "/empty.route", profile);
}

SimConfig two_robots(const std::string& profile) {
    return load_config(fixtures + "/sim_params_min.json", fixtures + "/field_two.json", fixtures + "/empty.route",
                       fixtures + "/empty.route", profile);
}

std::vector<std::string> cells(const std::string& line) {
    std::vector<std::string> out;
    std::stringstream ss(line);
    std::string c;
    while (std::getline(ss, c, ','))
        out.push_back(c);
    if (!line.empty() && line.back() == ',')
        out.push_back("");
    return out;
}

std::string last_line(const std::string& csv) {
    const std::size_t end = csv.find_last_not_of('\n');
    return csv.substr(csv.rfind('\n', end) + 1, end - csv.rfind('\n', end));
}

auto forward(double volts) {
    return [volts](int) { return std::make_pair(volts, volts); };
}

}  // namespace

TEST_CASE("Integration: Profil none liefert perfekte Sensoren (Odometrie, GPS = Wahrheit)") {
    SimConfig cfg = one_robot("none");
    std::ostringstream s, r;
    run_match(cfg, 3, s, r);
    const auto c = cells(last_line(s.str()));
    REQUIRE(c.size() == 27);
    CHECK(std::stod(c[col_x]) == doctest::Approx(-60.0));
    CHECK(std::stod(c[col_y]) == doctest::Approx(12.5));
    CHECK(std::stod(c[col_h]) == doctest::Approx(90.0));
    CHECK(std::stod(c[col_gx]) == doctest::Approx(-60.0));
    CHECK(std::stod(c[col_gy]) == doctest::Approx(12.5));
}

TEST_CASE("Integration: Roboter ohne Startpose bekommt keine Sensorwerte") {
    SimConfig cfg = one_robot("realistic");
    std::ostringstream s, r;
    run_match(cfg, 3, s, r);
    const auto c = cells(last_line(r.str()));
    REQUIRE(c.size() == 27);
    CHECK(c[col_x].empty());
    CHECK(c[col_gx].empty());
    CHECK(c[col_tx].empty());
}

TEST_CASE("Integration: Fahrt geradeaus, Odometrie folgt der Wahrheit (Profil none)") {
    SimConfig cfg = two_robots("none");
    cfg.stacker.open_loop = [](int t_ms) { return t_ms < 400 ? std::make_pair(6.0, 6.0) : std::make_pair(0.0, 0.0); };
    std::ostringstream s, r;
    run_match(cfg, 1, s, r);
    const auto c = cells(last_line(s.str()));
    const double tx = std::stod(c[col_tx]);
    CHECK(tx > -20.0 + 3.0);  // ist gefahren
    CHECK(std::stod(c[col_x]) == doctest::Approx(tx).epsilon(0.01));
}

TEST_CASE("Integration: Feldrand hält den Roboter, Ereignis COLLISION:wall") {
    SimConfig cfg = one_robot("none");
    cfg.stacker.open_loop = forward(6.0);  // von (-60, 12,5) mit Blick +y
    std::ostringstream s, r;
    const RunResult res = run_match(cfg, 1, s, r);
    const auto c = cells(last_line(s.str()));
    // Box 17,5 lang entlang +y: Vorderkante bei y + L/2 darf nicht über 72.
    CHECK(std::stod(c[col_ty]) <= 72.0 - cfg.stacker.length_in / 2.0 + 1e-6);
    CHECK(std::stod(c[col_ty]) > 72.0 - cfg.stacker.length_in / 2.0 - 1.0);  // und liegt an der Wand
    CHECK(s.str().find("COLLISION:wall") != std::string::npos);
    CHECK(res.wall_contacts == 1);  // ein Kontakt, auch wenn der Roboter weiter gegen die Wand fährt
}

TEST_CASE("Integration: Fahrt ueber Center-Stacks (Objekte) erzeugt keine Kollision und aendert die Pose nicht") {
    // SIM-30 / ADR-025 Nachtrag: Cups und Pins sind fuer die Kollision unsichtbar.
    auto drive_through = [](const std::string& field_file) {
        SimConfig cfg = load_config(fixtures + "/sim_params_min.json", fixtures + "/" + field_file,
                                    fixtures + "/empty.route", fixtures + "/empty.route", "none");
        cfg.stacker.open_loop = [](int t_ms) {
            return t_ms < 3800 ? std::make_pair(6.0, 6.0) : std::make_pair(0.0, 0.0);
        };
        std::ostringstream s, r;
        const RunResult res = run_match(cfg, 1, s, r);
        return std::make_tuple(s.str(), res, std::stod(cells(last_line(s.str()))[col_tx]));
    };
    const auto with = drive_through("field_stacks.json");
    const auto without = drive_through("field_min_start0.json");
    const double end_x = std::get<2>(with);
    CHECK(end_x > 23.546 + 5.0);  // ist ueber beide Stacks hinweggefahren
    CHECK(std::get<0>(with).find("COLLISION") == std::string::npos);
    CHECK(std::get<1>(with).wall_contacts == 0);
    CHECK(std::get<1>(with).robot_contacts == 0);
    CHECK(std::get<0>(with) == std::get<0>(without));  // Pose und Log byte-gleich wie ohne Objekte
}

TEST_CASE("Integration: zwei Roboter fahren aufeinander zu, überlappen nie und melden COLLISION") {
    SimConfig cfg = two_robots("none");
    cfg.stacker.open_loop = forward(6.0);
    cfg.runner.open_loop = forward(6.0);
    std::ostringstream s, r;
    const RunResult res = run_match(cfg, 1, s, r);
    CHECK(res.robot_contacts == 1);
    CHECK(s.str().find("COLLISION:runner") != std::string::npos);
    CHECK(r.str().find("COLLISION:stacker") != std::string::npos);

    // Endlage: Boxen berühren sich höchstens (kein Eindringen über die Auflösetoleranz hinaus).
    const auto cs = cells(last_line(s.str()));
    const auto cr = cells(last_line(r.str()));
    const double gap =
        std::stod(cr[col_tx]) - std::stod(cs[col_tx]) - cfg.stacker.length_in / 2.0 - cfg.runner.length_in / 2.0;
    CHECK(gap > -0.01);
}

TEST_CASE("Integration: Footprint über 24 Zoll meldet RULE:SG2 beim Start") {
    SimConfig cfg = one_robot("none");
    cfg.stacker.length_in = 25.0;
    std::ostringstream s, r;
    const RunResult res = run_match(cfg, 1, s, r);
    CHECK(res.rule_violations == 1);
    CHECK(s.str().find("RULE:SG2") != std::string::npos);
}

TEST_CASE("Integration: Matchende 0:00 mit laufenden Motoren ist ein Regelverstoß, die Motoren stehen danach") {
    SimConfig cfg = two_robots("none");
    cfg.stacker.open_loop = [](int t_ms) { return t_ms < 100 ? std::make_pair(3.0, 3.0) : std::make_pair(0.0, 0.0); };
    cfg.runner.open_loop = forward(0.0);
    std::ostringstream s1, r1;
    CHECK(run_match(cfg, 1, s1, r1).rule_violations == 0);

    cfg.runner.open_loop = [](int t_ms) { return t_ms >= 59000 ? std::make_pair(2.0, 2.0) : std::make_pair(0.0, 0.0); };
    std::ostringstream s2, r2;
    const RunResult res = run_match(cfg, 1, s2, r2);
    CHECK(res.rule_violations == 1);
    CHECK(r2.str().find("RULE:MOTORS_END") != std::string::npos);
}

TEST_CASE("Integration: gleicher Seed = byte-gleiches Log, anderer Seed = anderes (Profil realistic)") {
    const auto run = [](std::uint64_t seed) {
        SimConfig cfg = two_robots("realistic");
        cfg.stacker.open_loop = [](int t_ms) {
            return t_ms < 2000 ? std::make_pair(4.0, 4.0) : std::make_pair(0.0, 0.0);
        };
        std::ostringstream s, r;
        run_match(cfg, seed, s, r);
        return s.str() + r.str();
    };
    CHECK(run(5) == run(5));
    CHECK(run(5) != run(6));
}

TEST_CASE("Integration: Schätzpose driftet mit realistischen Sensoren leicht von der Wahrheit weg") {
    SimConfig cfg = two_robots("harsh");
    cfg.stacker.open_loop = [](int t_ms) { return t_ms < 1000 ? std::make_pair(6.0, 6.0) : std::make_pair(0.0, 0.0); };
    std::ostringstream s, r;
    run_match(cfg, 2, s, r);
    const auto c = cells(last_line(s.str()));
    const double err =
        std::hypot(std::stod(c[col_x]) - std::stod(c[col_tx]), std::stod(c[col_y]) - std::stod(c[col_ty]));
    CHECK(err > 0.0);
    CHECK(err < 20.0);  // grob: Fehler bleibt in der Größenordnung des Modells
}
