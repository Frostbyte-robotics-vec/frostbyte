// frostbyte_sim: Kommandozeile der Simulation.
//   frostbyte_sim --stacker <route> --runner <route> [--runs N] [--seed S] [--out DIR]
//                 [--params sim/sim_params.json] [--field field/field.json]
// Schreibt je Lauf i: DIR/run<NNN>_stacker.csv und DIR/run<NNN>_runner.csv, Seed = S + i.

#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "sim/world.hpp"

#ifndef FB_BUILD
#define FB_BUILD "-"
#endif

namespace {

int usage() {
    std::fprintf(stderr,
                 "Aufruf: frostbyte_sim --stacker <route> --runner <route> [--runs N] [--seed S] [--out DIR]\n"
                 "                     [--params sim/sim_params.json] [--field field/field.json]\n");
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    std::string stacker, runner, out = "out/sim", params = "sim/sim_params.json", field = "field/field.json";
    long long runs = 1, seed = 1;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (i + 1 >= argc)
            return usage();
        const std::string v = argv[++i];
        if (a == "--stacker")
            stacker = v;
        else if (a == "--runner")
            runner = v;
        else if (a == "--out")
            out = v;
        else if (a == "--params")
            params = v;
        else if (a == "--field")
            field = v;
        else if (a == "--runs" || a == "--seed") {
            std::size_t used = 0;
            long long n = 0;
            try {
                n = std::stoll(v, &used);
            } catch (const std::exception&) {
                return usage();
            }
            if (used != v.size())
                return usage();
            (a == "--runs" ? runs : seed) = n;
        } else
            return usage();
    }
    if (stacker.empty() || runner.empty() || runs < 1 || seed < 0)
        return usage();

    try {
        frostbyte::sim::SimConfig cfg = frostbyte::sim::load_config(params, field, stacker, runner);
        cfg.build = FB_BUILD;
        if (!cfg.stacker.start)
            std::fprintf(stderr, "Hinweis: START_STACKER in field.json ist null (A-01) - Pose bleibt leer.\n");
        if (!cfg.runner.start)
            std::fprintf(stderr, "Hinweis: START_RUNNER in field.json ist null (A-01) - Pose bleibt leer.\n");
        std::filesystem::create_directories(out);
        for (long long i = 0; i < runs; ++i) {
            const auto run_seed = static_cast<std::uint64_t>(seed + i);
            char name[32];
            std::snprintf(name, sizeof name, "run%03lld_", i);
            // binär: '\n' auch unter Windows, damit die Dateien überall byte-gleich sind
            const std::filesystem::path dir(out);
            std::ofstream s(dir / (std::string(name) + "stacker.csv"), std::ios::binary);
            std::ofstream r(dir / (std::string(name) + "runner.csv"), std::ios::binary);
            if (!s || !r)
                throw std::runtime_error("Ausgabe nicht schreibbar: " + out);
            frostbyte::sim::run_match(cfg, run_seed, s, r);
        }
        std::printf("%lld Läufe nach %s geschrieben (Seed %lld bis %lld)\n", runs, out.c_str(), seed, seed + runs - 1);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Fehler: %s\n", e.what());
        return 1;
    }
    return 0;
}
