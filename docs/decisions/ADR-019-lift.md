# ADR-019: Lift-Subsystem (Homing, Höhe anfahren, Decke)

Status: angenommen (2. Nov 2026), Ticket M-01. Der Advisor (Opus) hat vorgegeben: sichere Zustände zuerst (vor Homing und im Fehler 0 V mit Bremse, Fahrbefehle abgelehnt), Fahr-Timeout fällt nicht auf „Motor frei“ sondern auf Halten der gemessenen Höhe, Sensorausfall nur über neues Homing heilbar, Kalibriertabelle prüfen statt raten, nichtlineare Geometrie nicht schönrechnen.

## Entscheidung
- `lib/subsystems/lift.hpp` (`LiftSubsystem`) und `lib/util/monotone_table.hpp` (`MonotoneTable<5>`), reine Logik ohne PROS. Schnittstelle: `request_home/move/hold`, `set_ceiling_in`, `state()`, `update(now_s, LiftSensor) -> LiftOutput{volts, brake}`. Der Snapshot hat noch keine Lift-Felder (Plan: `update(snapshot)`); die Umstellung kommt mit der Verdrahtung.
- **Zustände:** `unhomed → homing → holding ⇄ moving`, dazu `error`. Vor dem Homing und im Fehler: 0 V, Bremse. `request_move`/`request_hold` geben dort `false` und ändern nichts.
- **Homing:** langsam zum unteren Anschlag (`home_v`), Stillstand trotz Spannung (`home_stall_deg_s` über `home_stall_s`, nach einer Anlaufzeit) setzt den Nullwinkel. Timeout → `error (home_timeout)`. Bekannte Grenze: Eine Blockade auf halber Höhe sieht aus wie der Anschlag und liefert einen falschen Nullpunkt; deshalb steht im Hardware-Test, das Homing aus der Mitte und bei künstlicher Blockade zu prüfen.
- **Kalibrierung:** Schlüssel ist der Sensorwinkel relativ zu Home, also steckt die Übersetzung in der Tabelle (5 Punkte). Zwischen den Punkten monotone kubische Interpolation (Fritsch–Carlson): bei einer Schwenkarm-Geometrie (Höhe = 10 + 20·sin θ, 0–90°) bleibt der Fehler bei 0,23 in, linear wären es 0,38 in (Test misst beides). `angle_for_height` ist die Umkehrung durch Halbieren (feste 50 Schritte). Außerhalb der Tabelle wird begrenzt, nie extrapoliert. Eine ungültige Tabelle (nicht endlich, nicht streng steigend) oder min ≥ max macht den Lift dauerhaft zu `error (config)`.
- **Regelung:** P-Anteil auf den Winkel plus Haltespannung `kg_v` gegen die Schwerkraft, Spannung je Richtung begrenzt. Kein Import von `control/pid.hpp` (liegt noch auf ticket/d-05).
- **Endlagen:** Ziele werden auf `[min_in, min(max_in, Decke)]` begrenzt; jenseits von `max_in/min_in ± margin_in` wird die Spannung nach außen gesperrt.
- **Decke:** `set_ceiling_in` ist ein Eingang (Endgame-Höhe ab 10 s, RuleGuard M-07 entscheidet, nicht der Lift). Sinkt sie unter die Haltehöhe, fährt der Lift von selbst nach unten (Zustand `moving`, mit Timeout). Steigt sie wieder, bleibt der Lift stehen, bis ein neuer Befehl kommt.
- **Timeouts:** Jeder Fahrbefehl hat `move_timeout_s`. Danach wechselt der Lift nach `holding` mit der gemessenen Höhe als Ziel und `timed_out() == true` (Lift nie spannungslos, sonst fällt er). `holding` selbst hat kein Timeout, das ist der Normalzustand; „jeder Befehl hat ein Timeout“ gilt für die Fahr- und Homing-Befehle.
- **Sensorausfall** (`ok == false` oder NaN): `error (sensor)`, 0 V, Bremse, Lage verloren; erst ein neues Homing macht den Lift wieder fahrbar.

## Grenzen / offen
- Die Sim (`sim::Joint`) folgt einem Sollwert und keiner Spannung; der Lift ist daher nicht in `sim/world.cpp` verdrahtet (die Datei ändert auch ticket/d-05). Getestet wird gegen eine kleine Strecke im Test (Spannung → Geschwindigkeit, Schwerkraft, harte Anschläge). Verdrahtung in die Sim und an die HAL: Folgeticket (nach F-07 Ports).
- Ob der Runner einen Lift hat, ist offen (config/runner.hpp); nichts angelegt.
- Die Parameter in `config/` sind `TODO(HW)`; Tests nehmen eigene Werte.
