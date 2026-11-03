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
Korrektur nach Advisor-Rat: Der Pages-Bericht kommt aus `frostbyte_sim --demo --runs 100`, nicht aus Routen; echte Routen laufen in der Simulation ohnehin erst mit vermessenem Feld (A-09, C-06 blockiert) und dem Routenspieler (A-03). Darum erzeugt hier das Demo den Spielzustand, A-03 hängt später an dieselbe Schnittstelle (ADR-026). Regeln: CLAUDE.md, docs/rules_scoring.md, ADR-024.

## Hardware-Prüfung
—

## Übergabe
Fertig, Branch ticket/sim-20 (Stand von arjanssen übernommen). sim/game.* (Zyklen über ObjectModel, Scorer bei Änderung), Midfield aus field.json (45°-Quadrat, Roboterumrisse je Logzeile, Score(n)), Besitz je Roboter (has_pin/has_cup von Zyklusstart bis -ende), score in beiden Logs (leer ohne Spielzustand), Bericht-Hinweis, ADR-026, `spiel` in sim_params.json (quelle: schaetzung). Tests: exakter Endstand 60 = Handrechnung, Midfield-Geometrie, Besitz, Midfield im Log; Tests hängen nicht an Platzhaltern (Demo-Test setzt Cup-Lage und Midfield selbst). Test-Fix: strip in test_sim_game_log.cpp schrieb bei leerer score-Spalte außerhalb des Vektors (Absturz unter MinGW). Lokal grün: Laptop-Tests (307), Format, board validate, Python-Tests. PROS-Build (make ROBOT=...) scheitert auf diesem Windows-Rechner an einer Umgebungsmeldung (auch auf main); in lib/ und src/ wurde kein C++ geändert, CI baut. JS-Test (jsc) lokal nicht gelaufen; test_livesim.py grün. Kriterium 3 (summary.json): Kette aus C++-Test (Endstand im Log) und test_sim_report.py (P10/Median/P90 aus Logs mit score); kein Test, der ein Szenario bis in summary.json führt. Parallele Lösung ticket/sim-20-davidcop verworfen; Löschen ist Sache des Menschen. Demo- und viewer-demo-Logs tragen jetzt eine score-Spalte. Am Roboter nichts zu prüfen. Danach: A-03 (RoutePlayer), Hinweise des Advisors in git show fc35e54.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-03 · arjanssen (KI) · Ticket angelegt: SIM-10 lieferte nur die Funktion, kein Einhängen (Hinweis von arjanssen zu report.html) · –
2026-11-03 · arjanssen (KI) · Game + score im Log + Bericht + ADR-026; Übergabe an DAVIDCOPCOP · 
2026-11-03 · DAVIDCOPCOP (KI) · Übernommen von arjanssen: Endstand-Test, Test-Fix, schaetzung-Quelle · –
2026-11-03 · DAVIDCOPCOP (KI) · Midfield aus field.json, Besitz in has_pin/has_cup, Endstand-Test, Tests ohne Platzhalter · –
