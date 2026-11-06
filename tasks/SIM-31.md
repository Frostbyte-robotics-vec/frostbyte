---
id: SIM-31
phase: "5"
titel: Intake-Rollenmodell mit Stau (SIM-28) in die Sim-Welt und den Zyklus verdrahten
status: done
owner: arjanssen
branch: ticket/sim-31
depends: []
hw: nein
---

## Ziel
SIM-28 liefert `sim/intake_model.*` (`IntakeRollers`) und testet den IntakeSubsystem gegen diese Strecke. Im Match-Sim (`world`/`cycle`) läuft der Intake aber weiter über das Ereignismodell (SIM-04, `boden_einheit`/`loader_einheit`). Hier wird das Rollenmodell eingehängt: Aufnahmen können klemmen, der Impuls löst oder nicht, ein Dauerstau endet im Log als Fehler und kostet Zeit.

## Fertig, wenn …
- Im Match-Sim klemmt eine Aufnahme mit der Wahrscheinlichkeit aus `ereignisse.intake_stau`; Stau, Impuls und `error (jam)` erscheinen als Ereignisse im Log (Format nur mit neuer Versionsnummer ändern, docs/log_format.md)
- Gleicher Seed = byte-gleiches Ergebnis; Tests mit eigenen Wahrscheinlichkeiten (0/1), nicht von Platzhalterwerten abhängig
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün

## Kontext
docs/decisions/ADR-037-intake.md (Nachtrag SIM-28), sim/intake_model.*, sim/objects.*, sim/game.cpp. Baut auf SIM-28 auf (PR muss gemergt sein; liegt sim/intake_model.hpp nicht auf main → überspringen).

## Hardware-Prüfung
—

## Übergabe
Fertig, PR offen. Stau läuft in Game::start_intake über das echte IntakeSubsystem (ADR-037 Nachtrag SIM-31). Test: ctest --test-dir build-test (test/test_sim_game_intake_jam.cpp). Werte klemmt/loest_nach_impuls bleiben Schätzung bis M-05.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-05 · arjanssen (KI) · Ticket angelegt als Folge von SIM-28 · –
2026-11-05 · arjanssen (KI) · Stau im Match-Sim: Game::run_jam, Token INTAKE:JAM/IMPULSE/ERROR_JAM, Tests, ADR · ticket/sim-31
