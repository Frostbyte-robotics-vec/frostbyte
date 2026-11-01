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
Stand 2026-11-01: Review PR #9 (Punkte 1–4) eingearbeitet, wartet auf erneute Prüfung (Status review).
Laptop-Tests grün (47 Fälle), CLI-Zeile aus CLAUDE.md: 40 Dateien, zweiter Lauf byte-gleich; beide Roboter bauen; validate ok.
- 1: sim/json.cpp – Zahl streng nach JSON-Grammatik abgegrenzt, dann strtod (C-Locale, kein setlocale; ADR-007).
  Tests: 0.1 exakt, 0x10/+1/.5/1./1e/-/1e999 abgelehnt. Neuer CI-Job tests-macos auf macos-15 (Apple clang 17):
  war mit dem alten Code an json.cpp:127 rot (geprüft). macos-latest (AppleClang 21) hätte den Fehler NICHT gezeigt.
  Ob tests-macos Pflicht-Check wird, entscheidet ein Mensch (Branch protection).
- 2: RobotLog wirft, wenn format_row/format_header abschneiden; Tests in test_log_format (Rückgabe ≥ n, Puffer terminiert).
- 3: logfmt Writer::text ersetzt , " \r \n durch _ (ohne Allokation, nur im geschriebenen Bereich). log_format.md
  unverändert (v1 erlaubt diese Zeichen ohnehin nicht), Begründung in ADR-007. Test auch an der Abschneidegrenze.
- 4: eigene Meldung für match_ms % log_ms, Test mit Fixture sim_params_log7.json.
- Hinweis 5 (tools/sim_report.py fehlt) gehört zu SIM-06.
Vor Review: sim/json (eigener Leser), sim/world (eigener Leser), sim/world (1-ms-Raster aus sim_params welt, Steuerung alle steuer_ms, Log alle log_ms, Rng splitmix64), sim/main.cpp = build-test/frostbyte_sim, lib/core/log_format.hpp (gemeinsamer Log-v1-Schreiber für Roboter und Sim). Entscheidungen in ADR-007.
Tests: test_log_format, test_sim_json, test_sim_world (3001 Zeilen bei leerer Route, gleicher Seed = byte-gleich, Fixtures in test/fixtures/). CLI-Zeile aus CLAUDE.md läuft, Dateien out/sim/run<NNN>_<robot>.csv.
Advisor-Rat (vor der Umsetzung): Log-Schreiber nach lib/ (F-05 nutzt ihn), Dateischema vom Viewer ableiten, null-Posen leer lassen statt (0,0), Raster in ganzen ms aus sim_params, build= als Git-Hash, ADR-007.
Offen/später: Routen werden nur gelesen (Interpretation A-03 RoutePlayer), Fahrmodell SIM-02, Sensoren SIM-03. Startposen in field.json sind null (A-01) → Pose-Spalten leer.
Am echten Roboter: nichts (reines Laptop-Ticket); F-05 soll lib/core/log_format.hpp verwenden.
Bauen unter Windows: Tests mit C:\msys64\ucrt64\bin im PATH; make in PowerShell mit $env:TMP=$env:TEMP (docs/notes.md).

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-01 · DAVIDCOPCOP · Sim-Grundgerüst, JSON-Leser, Log-v1-Schreiber, CLI, ADR-007 · Branch ticket/sim-01
2026-11-01 · DAVIDCOPCOP · Advisor-Review: Namespace logfmt, ARM-Kompilierprüfung, CLI-Fehlerbehandlung · Branch ticket/sim-01
2026-11-01 · DAVIDCOPCOP · Review PR #9 eingearbeitet (1–4), CI-Job tests-macos (macos-15) · 1006a4a
