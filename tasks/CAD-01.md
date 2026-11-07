---
id: CAD-01
phase: "1"
titel: Fusion-360-Nachbau des Runners als Script ins Repo (echte VEX-Teile, Gelenke, Montageprüfung, Odometrie)
status: review
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
Stand: fertig, PR offen. cad/runner/ (Build-Script, Prüfscript, ANLEITUNG.md, Ansicht von unten), ADR-040.
Prüfung im echten Fusion (07.11.2026, pruefung.txt): Startvolumen 14,77 × 14,88 × 14,70″ (≤ 15″), Kollisionen Startstellung 0,
Lift +12 … +72° je 0 Kollisionen, Schrauben 129 (frei 72, Vormontage 16, erst vor späterer Baugruppe 41, nicht erreichbar 0).
Odometrie-Schwingen in beiden Endlagen (±0,04 rad) ohne Kollision. Tiefe Y hat nur 0,12″ Reserve.
Am echten Roboter offen: Gummiband für die Schwingen (nicht modelliert), Druckteile aus PETG drucken und Passung prüfen,
alle Maße sind CAD-Startwerte (Einmessen: M-14, M-11).

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-07 · DAVIDCOPCOP (KI) · Ticket angelegt und übernommen; Script in Fusion gebaut und geprüft · –
2026-11-07 · DAVIDCOPCOP (KI) · cad/runner/ + ADR-040, PR #88 · 6e4ea65
