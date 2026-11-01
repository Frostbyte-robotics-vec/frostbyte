# ADR-007: Simulation – Grundgerüst, eigener JSON-Leser, gemeinsamer Log-Schreiber

Status: angenommen (1. Nov 2026), Ticket SIM-01. Der Advisor (Opus) hat den Entwurf vor der Umsetzung geprüft.

## Entscheidung
- **Kein JSON-Paket.** `sim/json.hpp/.cpp` ist ein kleiner eigener Leser (Objekte in Dateireihenfolge, Arrays, Zahlen über `std::from_chars` unabhängig vom Gebietsschema, `\uXXXX` → UTF-8, Fehler als `std::runtime_error`). Er wird nur am Laptop gebaut. Grund: field.json und sim_params.json sind klein und einfach, und ein Fremdpaket bräuchte Download oder Vendoring.
- **Log-Zeilen nur an einer Stelle.** `lib/core/log_format.hpp` (header-only, Namespace `frostbyte::logfmt`, ohne PROS, ohne Allokation) formatiert Kopfzeile und Zeilen nach `docs/log_format.md`. Der Logger am Roboter (F-05) nutzt dieselbe Funktion. Zahlen mit `%.3f`, keine `-0.000`, leere Zelle bei NaN, `no_flag` oder `nullptr`.
- **Determinismus.** Die Weltzeit zählt in ganzen ms. Steuerung läuft bei `t_ms % steuer_ms == 0`, Log bei `t_ms % log_ms == 0`, `t_s` wird pro Zeile neu berechnet. Das Raster (`schritt_ms`, `steuer_ms`, `log_ms`) kommt aus `welt` in `sim/sim_params.json`. Zufall nur über `sim::Rng` (splitmix64), nie über `std::*_distribution`. Dateien werden binär geschrieben (`\n` auch unter Windows).
- **Null-Koordinaten.** Ist eine Startpose in field.json `null`, erfindet die Simulation keine Pose: die Pose-Zellen bleiben leer, `frostbyte_sim` gibt einen Hinweis aus.
- **Ausgabe.** Je Lauf und Roboter eine Datei `<out>/run<NNN>_<robot>.csv` (passt zum Viewer aus SIM-07: eine Datei = ein Roboter laut Kopfzeile). Seed von Lauf i = `--seed` + i (Standard 1). `build=` ist der kurze Git-Hash zur CMake-Konfigurationszeit, ohne git `-`.
- **Build.** `test/CMakeLists.txt` baut zusätzlich `sim/*.cpp` (ohne `sim/main.cpp`) als `frostbyte_simlib` und das Programm `build-test/frostbyte_sim` (ergänzt ADR-004). Tests verwenden eigene Fixtures in `test/fixtures/`, nie die Platzhalter aus field.json oder Schätzwerte aus sim_params.json.

## Folgen
- 60 s Matchlänge steht als Spielregel in `sim/world.hpp` (`match_ms`), nicht in sim_params.json.
- Routen werden nur gelesen (Kommentare entfernt); interpretiert werden sie erst mit dem RoutePlayer (A-03) bzw. SIM-02 ff.
- `build=` ändert sich erst nach erneutem `cmake -S test -B build-test`; für Byte-Vergleiche gleiche Build-Konfiguration verwenden.
