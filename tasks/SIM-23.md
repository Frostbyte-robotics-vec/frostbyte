---
id: SIM-23
phase: "5"
titel: Toggles in der Simulation modellieren (Anfangszustand, Dauer, Punkte)
status: review
owner: arjanssen
branch: ticket/sim-23
depends: [SIM-22]
hw: nein
---

## Ziel
Die Simulation kennt `toggle` aus der Route noch nicht (ADR-032): der Befehl kostet 0 s und die Toggles stehen von Anfang an auf der Zielfarbe. Damit ist der Gewinn der Toggles gratis, und P10 der Skills-Routen (A-05) ist zu hoch bewertet (nicht zu niedrig). Entstanden aus A-05.

## Fertig, wenn …
- `toggle` aus einer Route löst im Spielzustand die Zustandsänderung aus (Dauer aus `sim_params.json`) und erscheint im `score` beider Logs
- Anfangszustand der Toggles mit Manual-Quelle in `sim/sim_params.json` (kein geratener Wert), Dauer als `schaetzung` gekennzeichnet
- Gleicher Seed = byte-gleiche Logs, Tests ohne `schaetzung`-Werte als Wahrheit
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün

## Kontext
tasks/A-05.md (Übergabe), ADR-026, ADR-032, docs/route_format.md.

## Hardware-Prüfung
—

## Übergabe
Implementiert auf ticket/sim-23, ADR-033. Nicht enthalten: Fahrt zum Toggle und Reichweite (SIM-25). Nicht enthalten: Fahrt zum Toggle und Reichweite (SIM-25). Am Roboter zu prüfen: Dauer des Toggle-Drehens (`spiel.toggle_dauer_s`, Schätzung 2,5 s) und ob der Arm den Toggle sicher auf alle drei Stellungen setzt.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-04 · arjanssen (KI) · Ticket angelegt (aus A-05) · –
2026-11-04 · arjanssen (KI) · toggle im Spielzustand, Anfangszustand neutral (Regelheft Appendix A), ADR-033, Tests · ticket/sim-23
