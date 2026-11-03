#pragma once

#include <functional>
#include <iosfwd>
#include <string>
#include <utility>
#include <vector>

// Eingabeverlauf (SIM-17, ADR-025 Nachtrag): feste Motorspannungen statt Route, damit frostbyte_sim und der
// Browser-Livesim denselben Verlauf fahren. CSV mit Kopfzeile "t_ms,robot,left_V,right_V"; '#'-Zeilen und Leerzeilen
// zählen nicht. Die Spannung gilt ab t_ms bis zur nächsten Zeile desselben Roboters (davor und ohne Zeile 0 V, nach
// der letzten Zeile bleibt sie). t_ms ist Vielfaches des Steuertakts und je Roboter nicht fallend.
namespace frostbyte::sim {

struct VoltRow {
    int t_ms = 0;
    double left_V = 0.0;
    double right_V = 0.0;
};

struct InputScript {
    std::vector<VoltRow> stacker;
    std::vector<VoltRow> runner;
};

// Wirft std::runtime_error mit Zeilennummer bei jedem Formfehler.
InputScript parse_input_script(std::istream& in, int steuer_ms);
InputScript load_input_script(const std::string& path, int steuer_ms);

// Spannung (links, rechts) zur Zeit t_ms; für RobotSetup::open_loop.
std::function<std::pair<double, double>(int t_ms)> input_open_loop(std::vector<VoltRow> rows);

}  // namespace frostbyte::sim
