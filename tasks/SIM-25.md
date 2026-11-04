---
id: SIM-25
phase: "5"
titel: Toggle-Reichweite in der Simulation (Befehl toggle nur nahe am Toggle)
status: done
owner: arjanssen
branch: ticket/sim-25
depends: [SIM-23]
hw: nein
---

## Ziel
`toggle` setzt in der Simulation jeden Toggle von der Stelle aus, an der der Roboter steht (ADR-033). Die vier Toggles liegen an vier Wänden (field.json `toggles`, `wand_deg`); die Route A-05 v1 fährt vor den Toggles nur an eine Pose. Der Befehl soll abgelehnt werden (failed), wenn der Roboter zu weit vom Toggle steht, damit Routen hinfahren müssen. Der Wert für die Reichweite ist zu messen (Arm am Runner, M-11/TGL), bis dahin ist er eine Schätzung in `sim_params.json`. Entstanden aus SIM-23.

## Fertig, wenn …
- `toggle` wird abgelehnt, wenn der Abstand der Roboterpose zum Toggle (field.json `x_in`, `y_in`) größer als die Reichweite aus `sim_params.json` ist; Test mit eigener Reichweite im Fixture
- Die Reichweite steht in `sim_params.json` mit `quelle` (Schätzung, bis gemessen), kein Test hängt an diesem Wert
- Gleicher Seed = byte-gleiche Logs; ein ADR (oder Ergänzung zu ADR-033) hält die Entscheidung fest
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün

## Kontext
ADR-033, tasks/SIM-23.md, field/field.json (`toggles`), routes/skills_runner.route.

## Hardware-Prüfung
—

## Übergabe
Stand: fertig, Tests grün, Advisor-Review eingearbeitet (wahre Pose statt Schätzung, Determinismus geprüft). Folge: Route v1 skills_runner fällt in der Simulation von 56 auf 0 Punkte (kein goto vor den Toggles); in tasks/A-05.md vermerkt. ADR-033 ergänzt.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-04 · arjanssen (KI) · Ticket angelegt (aus SIM-23, Advisor-Hinweis) · –
2026-11-04 · arjanssen (KI) · Reichweite in Game, world, route_runner, sim_params, Tests, ADR-033 · 6d23208
