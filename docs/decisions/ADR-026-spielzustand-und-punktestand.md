# ADR-026: Spielzustand und Punktestand in der Simulation

Status: angenommen (3. Nov 2026), Ticket SIM-20. Der Advisor (Opus) hat vorgegeben: SIM-20 vor und unabhängig von A-03; nur das Demo erzeugt Spielzustand (der Bericht der Pages-Seite kommt aus `frostbyte_sim --demo`), eigene Zufallsströme, `score_skills()` nur bei Zustandsänderung, die vom Regeltext nicht gelieferten Werte als ausdrückliche Eingaben.

## Entscheidung
- `sim/game.hpp/.cpp`: `GameSetup` (Goals mit Zone/Toggle/Stapel, Zyklen `{t_ms, robot, goal, level, source}`, Ereignis- und Mechanismusparameter, Annahmen) und `Game`. `Game::step(t_ms)` startet fällige Zyklen über `ObjectModel::run_cycle` (Dauer und Zufall aus SIM-04); ein erfolgreicher Zyklus legt nach seiner Dauer ein Teil auf den Stapel (SC2 im Wechsel Pin, Cup, Pin, …), danach rechnet `score_skills()` neu. Der Punktestand ist zwischendurch zwischengespeichert.
- Die Zyklen sind Vorgaben des Szenarios (Zeit, Goal, Ebene). Die Roboter fahren dafür nirgends hin; Fahrt und Zyklus sind im Demo entkoppelt. Der Routenspieler (A-03) wird später dieselben Zyklen aus Routen auslösen; dafür ist nur `GameSetup::cycles` zu füllen (oder `Game` um einen Aufruf `start_cycle` zu erweitern).
- Zyklus, der beginnt, während der Roboter noch einen fährt: übersprungen und gezählt (`skipped_cycles`), nie stillschweigend. Zyklus, der bei oder nach 0:00 fertig wird, zählt nicht.
- `run_match` schreibt den Teampunktestand in die Spalte `score` **beider** Roboterlogs jeder Zeile. Ohne Spielzustand (`GameSetup::goals` leer, alle Routenläufe bis A-03) bleibt die Spalte leer; der Bericht zeigt dann „–“ und `target_p10` bleibt „nicht geprüft“. Log-Format bleibt v1.
- Zufall: zwei eigene Seeds für die `ObjectModel`, nach den beiden Sensor-Seeds aus dem Match-Seed gezogen. Die Posen und alle anderen Spalten bleiben mit und ohne Spielzustand byte-gleich (Test `test_sim_game_log.cpp`).
- Annahmen ohne Regeltext stehen als Eingaben in `sim_params.json` (`spiel`, Quelle „annahme“): `cup_undurchsichtig_unten` (ADR-024, Quelle `schaetzung`).
- Roboter im Midfield: `GameSetup::midfield` kommt aus field.json (`midfield`: Quadrat 48″, um 45° gedreht; fehlt der Block, zählt kein Roboter). `run_match` zählt je Logzeile die Roboterumrisse, die es berühren (RSC3b „zumindest teilweise“, höchstens 2), und fragt `Game::score(n)`; der Stand ist also „wäre das Match jetzt zu Ende“ und kann sinken, wenn ein Roboter das Midfield verlässt. Die Midfield-Posen MIDFIELD_S/R (A-09) werden dafür nicht gebraucht.
- Besitz: ab Start eines Zyklus bis zu seinem Ende hält der Roboter Pin oder Cup (nach der Stapelfolge wie beim Ablegen, Teile auf dem Weg mitgezählt); `has_pin`/`has_cup` in der Logzeile. Ein Roboter fährt nur einen Zyklus zugleich, ein zweiter wird übersprungen; SG6 (1 Pin + 1 Cup) kann deshalb nicht überschritten werden, es gibt keine Prüfung, die nie auslöst.
- Pinfarbe: Farbe des Quadranten, obere Hälfte gelb, wenn `yellow_up` (Platzhalter für die Demo).
- Demo (`apply_demo(cfg, params_path)`): zwei Goals (rot, blau), je Roboter alle 10 s ein Zyklus Loader → Ebene 0. Goals, Zeiten und Ebene sind Platzhalter wie die Startposen, nicht aus `field.json`.
- Bericht: Punkte als Teamsumme aus dem Demo gekennzeichnet („Platzhalter-Zyklen, keine Prognose“).

## Grenzen
- Die Demo-Punkte messen nichts über die Roboter: Dauer, Fehlschlag und Setzerfolg sind `schaetzung` (SIM-04), die Zyklen sind fest vorgegeben.
- Kein SG6-Check im Spielmodell (s. o.); der RuleGuard (M-07) prüft das am Roboter. Fehlgeschlagene Zyklen halten nichts. Keine Höhenprüfung, kein Toggle-Wechsel, nur Level 0..5 als Liftziel (linear zwischen Min und Max, Platzhalter).

## Am Roboter zu prüfen
- Nichts; erst wenn echte Logs die Ereigniswerte ersetzen (SIM-08).
