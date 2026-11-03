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
Fertig, Branch ticket/sim-20 (Übernahme von arjanssens Stand durch DAVIDCOPCOP): sim/game.* (Spielzustand, Zyklen über ObjectModel, score_skills bei Änderung), score in beiden Logs (leer ohne Spielzustand), Bericht-Hinweis, ADR-026, `spiel` in sim_params.json (quelle: schaetzung), Test mit exaktem Endstand (60 Punkte = Handrechnung, test_sim_game_log.cpp), Test-Fix (strip in test_sim_game_log.cpp schrieb bei leerer score-Spalte außerhalb des Vektors; Absturz unter MinGW). Lokal grün: Laptop-Tests (302), Format, board validate, Python-Tests. PROS-Build (make ROBOT=…) scheitert auf diesem Windows-Rechner an „Cannot create temporary file“ (Umgebung, auch auf main); es wurde kein C++ in lib/ oder src/ geändert, CI baut. JS-Test (jsc) hier nicht lokal gelaufen (kein Safari/jsc); tools/viewer/test_livesim.py ist grün. Eine parallele Lösung von DAVIDCOPCOP liegt auf ticket/sim-20-davidcop (verworfen, kann gelöscht werden). Demo-Logs und viewer-demo tragen jetzt eine score-Spalte. Am Roboter nichts zu prüfen. Danach: A-03 (RoutePlayer), dessen Hinweise des Advisors stehen in der Übergabe-Historie (git show fc35e54).

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-03 · arjanssen (KI) · Ticket angelegt: SIM-10 lieferte nur die Funktion, kein Einhängen (Hinweis von arjanssen zu report.html) · –
2026-11-03 · arjanssen (KI) · Game + score im Log + Bericht + ADR-026; Übergabe an DAVIDCOPCOP · 
2026-11-03 · DAVIDCOPCOP (KI) · Übernommen von arjanssen: Endstand-Test, Test-Fix, schaetzung-Quelle · –
