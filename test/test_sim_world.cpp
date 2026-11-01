#include <sstream>
#include <string>

#include "doctest/doctest.h"
#include "sim/world.hpp"

using namespace frostbyte::sim;

namespace {

const std::string fixtures = FB_TEST_FIXTURES;

SimConfig min_config() {
    return load_config(fixtures + "/sim_params_min.json", fixtures + "/field_min.json", fixtures + "/empty.route",
                       fixtures + "/empty.route");
}

int count_lines(const std::string& s) {
    int n = 0;
    for (char c : s)
        n += c == '\n';
    return n;
}

}  // namespace

TEST_CASE("Welt: Raster aus sim_params, Startpose aus field, null bleibt leer") {
    SimConfig cfg = min_config();
    CHECK(cfg.schritt_ms == 1);
    CHECK(cfg.steuer_ms == 10);
    CHECK(cfg.log_ms == 20);
    REQUIRE(cfg.stacker.start.has_value());
    CHECK(cfg.stacker.start->x_in == doctest::Approx(-60.0));
    CHECK(cfg.stacker.start->h_deg == doctest::Approx(90.0));
    CHECK_FALSE(cfg.runner.start.has_value());
    CHECK(cfg.stacker.route.empty());
}

TEST_CASE("Welt: leere Route läuft 60 s durch, Zeilen- und Taktzahl exakt") {
    SimConfig cfg = min_config();
    std::ostringstream s, r;
    RunResult res = run_match(cfg, 7, s, r);
    CHECK(res.log_rows == 3001);  // t = 0,00 … 60,00 alle 20 ms
    CHECK(res.control_ticks == 6001);
    CHECK(count_lines(s.str()) == 2 + 3001);
    CHECK(count_lines(r.str()) == 2 + 3001);
    CHECK(s.str().rfind("60.000,stacker,", std::string::npos) != std::string::npos);
}

TEST_CASE("Welt: Kopfzeile und erste Zeile nach Log v1") {
    SimConfig cfg = min_config();
    cfg.build = "abc123";
    std::ostringstream s, r;
    run_match(cfg, 42, s, r);
    std::istringstream in(s.str());
    std::string head, cols, first;
    std::getline(in, head);
    std::getline(in, cols);
    std::getline(in, first);
    CHECK(head == "# frostbyte-log v1 robot=stacker source=sim seed=42 build=abc123");
    CHECK(cols.rfind("t_s,robot,", 0) == 0);
    CHECK(first == "0.000,stacker,,,,,,,,-60.000,12.500,90.000,,,,,,,,,,,,,,,");
    std::istringstream rin(r.str());
    std::getline(rin, head);
    std::getline(rin, cols);
    std::getline(rin, first);
    CHECK(first == "0.000,runner,,,,,,,,,,,,,,,,,,,,,,,,,");
}

TEST_CASE("Welt: gleicher Seed -> byte-gleiche CSV, anderer Seed -> andere Kopfzeile") {
    SimConfig cfg = min_config();
    std::ostringstream s1, r1, s2, r2, s3, r3;
    run_match(cfg, 5, s1, r1);
    run_match(cfg, 5, s2, r2);
    run_match(cfg, 6, s3, r3);
    CHECK(s1.str() == s2.str());
    CHECK(r1.str() == r2.str());
    CHECK(s1.str() != s3.str());
}

TEST_CASE("Rng: gleicher Seed, gleiche Folge; Werte in [0, 1)") {
    Rng a(123), b(123), c(124);
    for (int i = 0; i < 100; ++i) {
        const double u = a.uniform();
        CHECK(u == b.uniform());
        CHECK(u >= 0.0);
        CHECK(u < 1.0);
    }
    CHECK(Rng(123).next() != c.next());
}

TEST_CASE("Welt: echte Repo-Dateien lassen sich einlesen") {
    const std::string root = FB_ROOT_DIR;
    CHECK_NOTHROW(load_config(root + "/sim/sim_params.json", root + "/field/field.json",
                              root + "/routes/skills_stacker.route", root + "/routes/skills_runner.route"));
}
