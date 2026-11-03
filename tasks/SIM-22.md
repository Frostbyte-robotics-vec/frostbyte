---
id: SIM-22
phase: "5"
titel: Routen-Zyklen im Spielzustand der Simulation (Dauer, Besitz, Punkte aus cycle/intake)
status: todo
owner:
branch:
depends: [A-04, SIM-20]
hw: nein
---

## Ziel
In der Simulation lösen `cycle` und `intake` der Route den Spielzustand aus (ADR-026, ADR-031): `Game` bekommt einen Aufruf `start_cycle`, der `SimRouteExecutor` meldet `done` erst nach der Zyklusdauer aus `ObjectModel::run_cycle` (Fehlschlag → `failed`), Besitz (`has_pin`/`has_cup`) und Punktestand (`score`) entstehen aus den Routen statt aus festen Demo-Zyklen. Entstanden aus A-04, dort sind diese Befehle Platzhalter, die sofort `done` melden.

## Fertig, wenn …
- Ein Zyklus der Route erscheint nach seiner Dauer im Stapel des Goals und im `score` beider Logs; ein fehlgeschlagener Zyklus meldet `failed` (on_timeout greift)
- `level=auto` wird ohne geratenen Wert aufgelöst oder die Route wird abgelehnt
- Gleicher Seed = byte-gleiche Logs, Tests ohne `schaetzung`-Werte als Wahrheit
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün

## Kontext
ADR-026 (Spielzustand), ADR-031 (Routen in der Simulation), `sim/route_runner.cpp` (`SimRouteExecutor::start`, Platzhalter).

## Hardware-Prüfung
—

## Übergabe
(aktueller Stand für die nächste Sitzung – wird bei jedem Sitzungsende überschrieben)

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-03 · DAVIDCOPCOP (KI) · Ticket angelegt, aus A-04 ausgelagert · –
