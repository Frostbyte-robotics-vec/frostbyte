---
id: SIM-01
phase: "1b"
titel: Welt-Grundgerüst: 1-ms-Schritt, Steuerung alle 10 ms, Seed, field.json laden, CLI frostbyte_sim, CSV nach docs/log_format.md
status: in_progress
owner: DAVIDCOPCOP
branch: ticket/sim-01
depends: [F-01, F-02]
hw: nein
---

## Ziel
Welt-Grundgerüst: 1-ms-Schritt, Steuerung alle 10 ms, Seed, field.json laden, CLI frostbyte_sim, CSV nach docs/log_format.md

## Fertig, wenn …
- gleicher Seed → byte-gleiche CSV; leere Route läuft 60 s durch
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün


## Kontext
Details im Plan: docs/plan.md (Ticket SIM-01). Regeln: CLAUDE.md.

## Hardware-Prüfung
—

## Übergabe
Stand (vor Review): fertig, alle Kriterien am Laptop erfüllt. sim/json (eigener Leser), sim/world (1-ms-Raster aus sim_params welt, Steuerung alle steuer_ms, Log alle log_ms, Rng splitmix64), sim/main.cpp = build-test/frostbyte_sim, lib/core/log_format.hpp (gemeinsamer Log-v1-Schreiber für Roboter und Sim). Entscheidungen in ADR-007.
Tests: test_log_format, test_sim_json, test_sim_world (3001 Zeilen bei leerer Route, gleicher Seed = byte-gleich, Fixtures in test/fixtures/). CLI-Zeile aus CLAUDE.md läuft, Dateien out/sim/run<NNN>_<robot>.csv.
Advisor-Rat (vor der Umsetzung): Log-Schreiber nach lib/ (F-05 nutzt ihn), Dateischema vom Viewer ableiten, null-Posen leer lassen statt (0,0), Raster in ganzen ms aus sim_params, build= als Git-Hash, ADR-007.
Offen/später: Routen werden nur gelesen (Interpretation A-03 RoutePlayer), Fahrmodell SIM-02, Sensoren SIM-03. Startposen in field.json sind null (A-01) → Pose-Spalten leer.
Am echten Roboter: nichts (reines Laptop-Ticket); F-05 soll lib/core/log_format.hpp verwenden.
Review (KI, PR #9) „ändern“ – Details im PR-Kommentar:
1. BLOCKIERT: sim/json.cpp:127 std::from_chars(double) fehlt in Apples libc++ → ganzer Laptop-Build scheitert auf macOS
   (CI nur Ubuntu). Fix: std::strtod (C-Locale, kein setlocale) oder from_chars hinter __cpp_lib_to_chars; optional macos-latest in ci.yml.
2. RobotLog::row ignoriert Abschneiden von format_row (Rückgabe ≥ n) → erkennen/melden, Test (wichtig für F-05).
3. Textzellen (cycle, route_step, event) ohne Komma-Schutz → ablehnen/ersetzen oder in log_format.md festhalten.
4. Fehlermeldung match_ms % log_ms in load_config ungenau.
In Arbeit (2026-11-01): Punkte 1–4.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-01 · DAVIDCOPCOP · Sim-Grundgerüst, JSON-Leser, Log-v1-Schreiber, CLI, ADR-007 · Branch ticket/sim-01
2026-11-01 · DAVIDCOPCOP · Advisor-Review: Namespace logfmt, ARM-Kompilierprüfung, CLI-Fehlerbehandlung · Branch ticket/sim-01
