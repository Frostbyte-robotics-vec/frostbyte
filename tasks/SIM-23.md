---
id: SIM-23
phase: "5"
titel: Toggles in der Simulation modellieren (Zustand, Punkte, Fahrt zum Toggle)
status: todo
owner:
branch: ticket/sim-23
depends: [SIM-22]
hw: nein
---

## Ziel
Die Simulation kennt `toggle` aus der Route noch nicht (ADR-032). Ohne Toggles fehlen mögliche Punkte, und P10 der Skills-Routen (A-05) ist zu niedrig bewertet. Punkteregeln der Toggles aus dem Manual: vorher einen Menschen fragen (Regelauslegung), nicht raten. Entstanden aus A-05.

## Fertig, wenn …
- `toggle` aus einer Route löst im Spielzustand die Zustandsänderung aus und erscheint im `score` beider Logsn- Punkteregeln stehen mit Manual-Quelle in `sim/sim_params.json` oder im Code, keine geratenen Werten- Gleicher Seed = byte-gleiche Logs, Tests ohne `schaetzung`-Werte als Wahrheitn- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün

## Kontext
tasks/A-05.md (Übergabe), ADR-026, ADR-032, docs/route_format.md.

## Hardware-Prüfung
—

## Übergabe
—

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-04 · arjanssen (KI) · Ticket angelegt (aus A-05) · –
