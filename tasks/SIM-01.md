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
(aktueller Stand für die nächste Sitzung – wird bei jedem Sitzungsende überschrieben)
Review (KI, PR #9) „ändern“ – Details im PR-Kommentar:
1. BLOCKIERT: sim/json.cpp:127 std::from_chars(double) fehlt in Apples libc++ → ganzer Laptop-Build scheitert auf macOS
   (CI nur Ubuntu). Fix: std::strtod (C-Locale, kein setlocale) oder from_chars hinter __cpp_lib_to_chars; optional macos-latest in ci.yml.
2. RobotLog::row ignoriert Abschneiden von format_row (Rückgabe ≥ n) → erkennen/melden, Test (wichtig für F-05).
3. Textzellen (cycle, route_step, event) ohne Komma-Schutz → ablehnen/ersetzen oder in log_format.md festhalten.
4. Fehlermeldung match_ms % log_ms in load_config ungenau.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
