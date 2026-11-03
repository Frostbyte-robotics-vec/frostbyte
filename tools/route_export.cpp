// Liest eine Route und schreibt sie als C++-Quelltext. Aufruf: frostbyte_route_export <field.json> <route> <symbol> <ausgabe.hpp>
// Bei einem Fehler: Meldung "<route>: Zeile N: ..." auf stderr, Exit 1, keine Ausgabedatei.
#include <fstream>
#include <iostream>
#include <sstream>

#include "sim/route_parser.hpp"

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "Aufruf: frostbyte_route_export <field.json> <route> <symbol> <ausgabe.hpp>\n";
        return 2;
    }
    try {
        const auto names = frostbyte::sim::route_names_from_field(argv[1]);
        std::ifstream in(argv[2], std::ios::binary);
        if (!in) {
            std::cerr << argv[2] << ": nicht lesbar\n";
            return 1;
        }
        std::stringstream ss;
        ss << in.rdbuf();
        const auto r = frostbyte::sim::parse_route(ss.str(), names);
        if (!r.ok()) {
            std::cerr << argv[2] << ": " << r.error << "\n";
            return 1;
        }
        std::ofstream out(argv[4], std::ios::binary);
        out << frostbyte::sim::route_to_cpp(r.cmds, argv[3]);
        return out.good() ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
