#include "sim/input_script.hpp"

#include <algorithm>
#include <fstream>
#include <istream>
#include <sstream>
#include <stdexcept>

namespace frostbyte::sim {

namespace {

[[noreturn]] void fail(int line_no, const std::string& what) {
    throw std::runtime_error("Eingabeverlauf Zeile " + std::to_string(line_no) + ": " + what);
}

std::string trim(const std::string& s) {
    const auto a = s.find_first_not_of(" \t\r");
    if (a == std::string::npos)
        return "";
    return s.substr(a, s.find_last_not_of(" \t\r") - a + 1);
}

double number(const std::string& cell, int line_no, const char* name) {
    std::size_t used = 0;
    double v = 0.0;
    try {
        v = std::stod(cell, &used);
    } catch (const std::exception&) {
        fail(line_no, std::string(name) + " ist keine Zahl: '" + cell + "'");
    }
    if (used != cell.size())
        fail(line_no, std::string(name) + " ist keine Zahl: '" + cell + "'");
    return v;
}

}  // namespace

InputScript parse_input_script(std::istream& in, int steuer_ms) {
    InputScript out;
    std::string raw;
    int line_no = 0;
    bool header = false;
    while (std::getline(in, raw)) {
        ++line_no;
        const std::string line = trim(raw);
        if (line.empty() || line[0] == '#')
            continue;
        if (!header) {
            if (line != "t_ms,robot,left_V,right_V")
                fail(line_no, "Kopfzeile 't_ms,robot,left_V,right_V' erwartet");
            header = true;
            continue;
        }
        std::vector<std::string> c;
        std::stringstream ss(line);
        std::string cell;
        while (std::getline(ss, cell, ','))
            c.push_back(trim(cell));
        if (c.size() != 4)
            fail(line_no, "4 Spalten erwartet");
        VoltRow row;
        const double t = number(c[0], line_no, "t_ms");
        if (t < 0 || t != static_cast<int>(t))
            fail(line_no, "t_ms muss eine ganze Zahl >= 0 sein");
        row.t_ms = static_cast<int>(t);
        if (row.t_ms % steuer_ms != 0)
            fail(line_no, "t_ms muss ein Vielfaches des Steuertakts (" + std::to_string(steuer_ms) + " ms) sein");
        row.left_V = number(c[2], line_no, "left_V");
        row.right_V = number(c[3], line_no, "right_V");
        std::vector<VoltRow>* rows = c[1] == "stacker" ? &out.stacker : c[1] == "runner" ? &out.runner : nullptr;
        if (!rows)
            fail(line_no, "robot muss stacker oder runner sein: '" + c[1] + "'");
        if (!rows->empty() && row.t_ms < rows->back().t_ms)
            fail(line_no, "t_ms läuft rückwärts");
        rows->push_back(row);
    }
    if (!header)
        fail(line_no, "Kopfzeile 't_ms,robot,left_V,right_V' fehlt");
    return out;
}

InputScript load_input_script(const std::string& path, int steuer_ms) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("Eingabeverlauf nicht lesbar: " + path);
    return parse_input_script(in, steuer_ms);
}

std::function<std::pair<double, double>(int)> input_open_loop(std::vector<VoltRow> rows) {
    return [rows = std::move(rows)](int t_ms) {
        // letzte Zeile mit t_ms <= t
        const auto it =
            std::upper_bound(rows.begin(), rows.end(), t_ms, [](int t, const VoltRow& r) { return t < r.t_ms; });
        if (it == rows.begin())
            return std::make_pair(0.0, 0.0);
        return std::make_pair((it - 1)->left_V, (it - 1)->right_V);
    };
}

}  // namespace frostbyte::sim
