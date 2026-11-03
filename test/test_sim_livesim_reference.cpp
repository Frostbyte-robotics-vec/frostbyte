// SIM-17: frostbyte_sim muss für den Eingabeverlauf aus den Fixtures weiter die eingecheckte Referenz liefern.
// Gegen dieselbe Referenz prüft tools/viewer/test_livesim.py den Browser-Livesim; zusammen halten beide Tests
// livesim.js und sim/ beieinander. Neu erzeugen (nur nach bewusster Modelländerung): python3 tools/make_livesim_reference.py
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "doctest/doctest.h"
#include "sim/input_script.hpp"
#include "sim/world.hpp"

using namespace frostbyte::sim;

namespace {

const std::string fixtures = FB_TEST_FIXTURES;

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::stringstream ss(line);
    std::string c;
    while (std::getline(ss, c, ','))
        out.push_back(c);
    if (!line.empty() && line.back() == ',')
        out.push_back("");
    return out;
}

// Zeilen der Log-CSV ohne Kommentar- und Spaltenzeile, Spalten: t_s=0, tx=9, ty=10, th=11, event=26.
std::vector<std::vector<std::string>> log_rows(const std::string& csv) {
    std::vector<std::vector<std::string>> rows;
    std::istringstream in(csv);
    std::string line;
    int n = 0;
    while (std::getline(in, line))
        if (++n > 2)
            rows.push_back(split(line));
    return rows;
}

}  // namespace

TEST_CASE("Livesim-Referenz: frostbyte_sim fährt den Eingabeverlauf wie eingecheckt (wahre Pose, Ereignisse)") {
    SimConfig cfg = load_config(fixtures + "/sim_params_min.json", fixtures + "/field_two.json", "", "", "none");
    const InputScript script = load_input_script(fixtures + "/livesim_input.csv", cfg.steuer_ms);
    cfg.stacker.open_loop = input_open_loop(script.stacker);
    cfg.runner.open_loop = input_open_loop(script.runner);
    std::ostringstream s, r;
    run_match(cfg, 1, s, r);
    const std::map<std::string, std::vector<std::vector<std::string>>> sim{{"stacker", log_rows(s.str())},
                                                                           {"runner", log_rows(r.str())}};

    std::ifstream ref(fixtures + "/livesim_reference.csv");
    REQUIRE(ref.good());
    std::string line;
    std::getline(ref, line);
    CHECK(line == "t_s,robot,tx_in,ty_in,th_deg,event");
    std::map<std::string, std::size_t> next;
    int rows = 0, events = 0;
    while (std::getline(ref, line)) {
        const auto c = split(line);
        REQUIRE(c.size() == 6);
        const auto& log = sim.at(c[1]);
        const std::size_t i = next[c[1]]++;
        REQUIRE(i < log.size());
        const auto& l = log[i];
        INFO("Zeile ", rows, " ", c[1], " t=", c[0]);
        REQUIRE(l.size() == 27);
        CHECK(l[0] == c[0]);
        CHECK(std::abs(std::stod(l[9]) - std::stod(c[2])) < 2e-3);
        CHECK(std::abs(std::stod(l[10]) - std::stod(c[3])) < 2e-3);
        CHECK(std::abs(std::stod(l[11]) - std::stod(c[4])) < 2e-3);
        CHECK(l[26] == c[5]);
        events += c[5].empty() ? 0 : 1;
        ++rows;
    }
    CHECK(rows == 2 * 501);  // 10 s im 20-ms-Raster je Roboter
    CHECK(events == 4);      // Roboter-Roboter (beide Seiten) und je ein Feldrand
}
