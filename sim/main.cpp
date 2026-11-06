// frostbyte_sim: Kommandozeile der Simulation.
//   frostbyte_sim --stacker <route> --runner <route> [--runs N] [--seed S] [--out DIR]
//                 [--params sim/sim_params.json] [--field field/field.json] [--noise none|realistic|harsh]
//   frostbyte_sim --input <verlauf.csv> [--runs N] [--seed S] [--out DIR] [--params ...] [--field ...] [--noise ...]
//                 feste Motorspannungen statt Routen (SIM-17, sim/input_script.hpp); Startposen in field.json nötig
//   frostbyte_sim --demo [--runs N] [--seed S] [--out DIR] [--noise ...]   Demo ohne Routen (Platzhalter-Posen, build=demo)
// Schreibt je Lauf i: DIR/run<NNN>_stacker.csv und DIR/run<NNN>_runner.csv, Seed = S + i.

#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "config/runner.hpp"
#include "config/stacker.hpp"
#include "sim/demo.hpp"
#include "sim/input_script.hpp"
#include "sim/world.hpp"

#ifndef FB_BUILD
#define FB_BUILD "-"
#endif

namespace {

int usage() {
    std::fprintf(stderr,
                 "Aufruf: frostbyte_sim --stacker <route> --runner <route> [--runs N] [--seed S] [--out DIR]\n"
                 "                     [--params sim/sim_params.json] [--field field/field.json] [--noise "
                 "none|realistic|harsh]\n"
                 "       frostbyte_sim --input <verlauf.csv> [--runs N] [--seed S] [--out DIR] ...\n"
                 "       frostbyte_sim --demo [--runs N] ...\n");
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    std::string stacker, runner, input, out = "out/sim", params = "sim/sim_params.json", field = "field/field.json";
    std::string noise = "realistic";
    bool demo = false;
    long long runs = 1, seed = 1;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--demo") {
            demo = true;
            continue;
        }
        if (i + 1 >= argc)
            return usage();
        const std::string v = argv[++i];
        if (a == "--stacker")
            stacker = v;
        else if (a == "--runner")
            runner = v;
        else if (a == "--input")
            input = v;
        else if (a == "--out")
            out = v;
        else if (a == "--params")
            params = v;
        else if (a == "--field")
            field = v;
        else if (a == "--noise")
            noise = v;
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
    if (demo && !input.empty())
        return usage();
    if ((!demo && input.empty() && (stacker.empty() || runner.empty())) || runs < 1 || seed < 0)
        return usage();

    try {
        frostbyte::sim::SimConfig cfg = frostbyte::sim::load_config(params, field, stacker, runner, noise);
        cfg.build = FB_BUILD;
        // Regler und Standard-Timeouts der Routen wie am Roboter (config/, A-04).
        cfg.stacker.control = {config::stacker::motion, config::stacker::lateral_pid, config::stacker::angular_pid,
                               config::stacker::route_player};
        cfg.runner.control = {config::runner::motion, config::runner::lateral_pid, config::runner::angular_pid,
                              config::runner::route_player};
        cfg.game.intake_cfg[0] =
            config::stacker::intake;  // Staus des Intakes laufen mit den Regelwerten des Roboters (SIM-31)
        cfg.game.intake_cfg[1] = config::runner::intake;
        if (demo)
            frostbyte::sim::apply_demo(cfg, params);
        if (!input.empty()) {
            const frostbyte::sim::InputScript script = frostbyte::sim::load_input_script(input, cfg.steuer_ms);
            cfg.stacker.open_loop = frostbyte::sim::input_open_loop(script.stacker);
            cfg.runner.open_loop = frostbyte::sim::input_open_loop(script.runner);
        }
        if (!demo && !cfg.stacker.start)
            std::fprintf(stderr, "Hinweis: START_STACKER in field.json ist null (A-01) - Pose bleibt leer.\n");
        if (!demo && !cfg.runner.start)
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
