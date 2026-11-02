// Schreibt docs/controls.md aus der Belegung in config/. Aufruf: frostbyte_controls_export docs/controls.md
#include <fstream>
#include <iostream>

#include "controls_doc.hpp"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Aufruf: frostbyte_controls_export <ausgabe.md>\n";
        return 2;
    }
    std::ofstream out(argv[1], std::ios::binary);
    out << frostbyte::controls_markdown();
    return out.good() ? 0 : 1;
}
