---
id: SIM-20
phase: "1b"
titel: Scorer in die Simulation einhängen (Spielzustand, score-Spalte im Log, Punkte im Bericht)
status: in_progress
owner: DAVIDCOPCOP
branch: ticket/sim-20
depends: [SIM-10, SIM-05]
hw: nein
---

## Ziel
SIM-10 hat nur die Funktion `score_skills()` (sim/scorer.*) mit Tests geliefert. Sie wird von nichts aufgerufen: `run_match`/`world` kennen keinen Spielzustand (welcher Pin/Cup steckt in welchem Goal) und schreiben die Log-Spalte `score` nie. Darum zeigt report.html (tools/sim_report.py) bei „Punkte“ immer „–“ und target_p10 bleibt „nicht geprüft“. Der Hinweis dort („sobald der Scorer (SIM-10) im Log steht“) ist irreführend, weil kein Ticket das Einhängen abdeckte (SIM-05 sagte „eigenes Ticket, noch anzulegen“).

## Fertig, wenn …
- Spielzustand in der Simulation (Pins/Cups in Goals, Besitz je Roboter, Roboter im Midfield) wird von Mechanismen/Demo/Routenspieler fortgeschrieben; Entwurf vorher als ADR (Advisor fragen)
- Die Simulation ruft `score_skills()` und schreibt den laufenden Punktestand in die Spalte `score` (Log-Format unverändert, v1)
- Test: Demo/Szenario mit bekanntem Endstand → erwarteter Wert im Log und in summary.json (P10/Median/P90 der Punkte)
- tools/sim_report.py zeigt Punkte und prüft target_p10; Hinweistext in report.html entfernt oder angepasst
- Gleicher Seed = byte-gleiches Ergebnis; Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün

## Kontext
Hängt praktisch an einer Quelle für Spielzustandsänderungen: Das Demo bewegt nur Roboter. Echte Punkte entstehen erst, wenn der Routenspieler (A-03) Aktionen auslöst; falls dafür A-03 nötig ist, in der Übergabe begründen oder einen Zwischenschritt (Demo setzt Pins direkt) wählen. Regeln: CLAUDE.md, docs/rules_scoring.md, ADR-024.

## Hardware-Prüfung
—

## Übergabe
—

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-03 · arjanssen (KI) · Ticket angelegt: SIM-10 lieferte nur die Funktion, kein Einhängen (Hinweis von arjanssen zu report.html) · –
