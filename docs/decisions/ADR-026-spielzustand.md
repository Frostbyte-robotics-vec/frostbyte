# ADR-026: Spielzustand in der Simulation (sim/game_state.*)

Status: angenommen (3. Nov 2026), Ticket SIM-20. Der Advisor (Opus) hat vorgegeben: Spielzustandsänderungen als skriptbare Ereignisse, weil der Routenspieler (A-03) noch nicht fertig ist; Zustand und Besitz über die vorhandene Regelprüfung (SG6); Tests dürfen nicht von Platzhaltern abhängen (Posen, Midfield-Lage, Fahrparameter).

## Entscheidung
- `sim/game_state.hpp/.cpp`: `GameSetup` (Goals mit Zone und Quadrant, Midfield-Quadrat, Startobjekte, Ereignisliste) und `GameState` (Stapel je Goal, Toggle je Quadrant, Besitz je Roboter). Das Einlesen aus field.json (`load_game_setup`) ist tolerant: fehlt `goals`, ist das Spiel leer.
- Ereignisse haben `t_ms`, Roboter und Art: `pick_pin(unten, oben)`, `pick_cup(opaque_lower)`, `place_pin(goal)`, `place_cup(goal)`, `set_toggle(quadrant, farbe)`. Die Cup-Ausrichtung (ADR-024, offen) steht am Ereignis, nicht als globaler Standard. `SimConfig::game.events` ist leer, solange niemand welche liefert; `apply_demo` und später der Routenspieler (A-03) füllen sie über dieselbe Schnittstelle.
- `run_match` wendet Ereignisse am ersten Log-Takt ab ihrem `t_ms` an, vor dieser Logzeile (Punktestand und Besitz ändern sich also nur auf dem log_ms-Raster). Besitz über SG6 (1 Pin + 1 Cup): ein dritter Gegenstand wird nicht aufgenommen und erzeugt `RULE:SG6` (zählt in `rule_violations`). Setzen ohne Besitz, unbekanntes Goal/Quadrant oder unsortierte Zeiten sind Fehler im Szenario (`std::runtime_error` beim Aufbau).
- Roboter im Midfield: ein Roboter zählt, sobald sein Umriss das Midfield-Quadrat (um 45° gedreht) berührt (RSC3b „zumindest teilweise“, max. 2).
- Die Spalte `score` ist der Stand „wäre das Match jetzt zu Ende“ (inkl. Roboter, die gerade im Midfield stehen), derselbe Teamstand in beiden Roboter-Logs; die letzte Zeile (60 s) ist das Ergebnis. `has_cup`/`has_pin` kommen aus dem Besitz. Ist das Spiel leer (keine Goals in field.json), bleiben die Spalten leer wie bisher.
- Startobjekte aus `objekte_start` mit `auf_goal` (gelbe Pins auf Goals) liegen von Beginn an im Stapel. Die Aufstellung ist laut field.json noch nicht vom Menschen gegen das Manual bestätigt; Tests hängen nicht daran.
- Log-Format unverändert (v1).

## Grenzen
- Ohne Routenspieler (A-03) entstehen Ereignisse nur aus Szenarien; das Demo (nur Fahren) liefert Punkte nur aus Startobjekten und Midfield-Standort. Echte Punktzahlen (target_p10) hängen an A-03.
- Keine Höhenprüfung und keine Toggle-Berührung durch Roboter (SC4b).
