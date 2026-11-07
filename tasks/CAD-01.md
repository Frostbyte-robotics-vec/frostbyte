---
id: CAD-01
phase: "1"
titel: Fusion-360-Nachbau des Runners als Script ins Repo (echte VEX-Teile, Gelenke, Montageprüfung, Odometrie)
status: in_progress
owner: DAVIDCOPCOP
branch: ticket/cad-01
depends: []
hw: nein
---

## Ziel
Der Runner wird in Fusion 360 automatisch aus echten VEX-Teilen (VEX-CAD Fusion 360 Library 2.0.0) gebaut, mit beweglichen Gelenken und einer Prüfung, dass jede Schraube in Montagereihenfolge erreichbar ist. Script, Prüfscript und Anleitung liegen im Repo, damit jeder das Modell neu erzeugen kann.

## Fertig, wenn …
- `cad/runner/FrostbyteRunner.py` (Build), `cad/runner/pruefung.py` (Prüfung), `cad/runner/ANLEITUNG.md` (Einrichten, Ausführen, Gelenke, Odometrie-Werte) im Repo
- Prüfung im echten Fusion gelaufen: Startvolumen ≤ 15″ in allen Richtungen, 0 Kollisionen in der Startstellung, 0 Kollisionen über den Liftweg, 0 nicht erreichbare Schrauben; Ergebnis im Ticket
- ADR für „CAD als Script im Repo“ (Ort, Einheiten, wie Werte aus dem CAD in config/ und sim_params.json kommen)
- Folge-Tickets für Software (D-11), Hardware (M-14) und Simulation (SIM-36) angelegt
- `python3 tools/board.py validate` grün

## Kontext
Vorlage: Runner in `tools/viewer/robots.js`. Material wie im Competition-Kit (Aluminium); eigene Alu-/Stahlteile folgen in späteren Iterationen.

## Hardware-Prüfung
—

## Übergabe
(aktueller Stand für die nächste Sitzung – wird bei jedem Sitzungsende überschrieben)

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-07 · DAVIDCOPCOP (KI) · Ticket angelegt und übernommen; Script in Fusion gebaut und geprüft · –
