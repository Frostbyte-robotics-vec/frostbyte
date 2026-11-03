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
pausiert – frei zur Übernahme durch DAVIDCOPCOP (Owner bleibt DAVIDCOPCOP; arjanssen hat die Arbeit vorgezogen, weil die eigene Übernahme zeitgleich fehlschlug – bitte nicht doppelt arbeiten).

Stand auf Branch ticket/sim-20 (Laptop-Tests, Format, board validate grün; 100 Demo-Läufe in 9,8 s): sim/game.* (Spielzustand, Zyklen über ObjectModel, score_skills bei Änderung), score in beiden Logs im Demo (leer ohne Spielzustand), Bericht-Hinweis angepasst, ADR-026, sim_params.json `spiel`, Tests test_sim_game.cpp, test_sim_game_log.cpp, HtmlHintTest.

Noch zu tun (Advisor-Review vor dem PR):
1. Test mit exaktem Endstand im Log: Fixture test/fixtures/sim_params_game.json (Kopie von sim_params_min.json plus `mechanismen` für beide Roboter, `spiel`, `ereignisse` mit Fehlschlag 0, sigma 0, Erfolg 1, Abzug 0, pin_gelb_oben.loader 1). run_match mit apply_demo(cfg, fixture); letzte score-Zelle beider Logs = fester Zahlenwert und = score_skills auf dem von Hand gebauten Zustand (je Goal 5 Teile: Pin, Cup, Pin, Cup, Pin; Zyklen bei 5–45 s und 7–47 s). Für summary.json im PR auf test_score_is_last_value und die P10-Tests verweisen.
2. In sim/sim_params.json `spiel.quelle` mit „schaetzung“ beginnen lassen (CLAUDE.md), z. B. „schaetzung (Annahme: Cup-Lage ungeklärt …)“; vorher grep -rn schaetzung tools sim test.
3. JS-Test gegen die echte sim_params.json (test_echte_parameter_und_feld) lokal über jsc laufen lassen (kein node hier): `/System/Library/Frameworks/JavaScriptCore.framework/Versions/A/Helpers/jsc` mit Shim `var console={log:print}`.
4. PR: erwähnen, dass Demo-Logs/viewer-demo jetzt eine score-Spalte tragen; robot-make hatte nichts zu bauen (kein C++ in lib/ oder src/ geändert). Dann /finish (Status review).

Danach: A-03 (RoutePlayer). Hinweise des Advisors dafür: depends C-01 → M-07 → F-04 (needs_hw); Überspringen nur mit Begründung in der Übergabe (Player hängt nur an einer Executor-Schnittstelle in lib/match, Adapter an den CycleController kommt mit C-01). Tick-basiertes update(), nichts Blockierendes, kein dynamischer Speicher, Verschachtelung über festen Stack mit body_len; jeder Befehl mit Timeout (timeout_s < 0 → Standard je Op, nie unendlich); retry begrenzt; `at` bricht die laufende Anweisung auch in `parallel` ab; bei 0:00 alles Halt; Namen und signal/wait_for über Schnittstellen, Tests mit eigener Lookup-Tabelle statt field.json.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-03 · arjanssen (KI) · Ticket angelegt: SIM-10 lieferte nur die Funktion, kein Einhängen (Hinweis von arjanssen zu report.html) · –
2026-11-03 · arjanssen (KI) · Game + score im Log + Bericht + ADR-026; Übergabe an DAVIDCOPCOP · 
