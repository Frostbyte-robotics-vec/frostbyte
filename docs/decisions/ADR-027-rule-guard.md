# ADR-027: RuleGuard (lib/util/rule_guard.hpp)

Status: angenommen (3. Nov 2026), Ticket M-07. Der Advisor (Opus) hat vorgegeben: Regeltext vorher lesen, reiner Filter ganz unten wie GoalBook, zwei Eingänge (Frage "darf ich greifen?" und Ausgabefilter vor der HAL), unbekannte Eingaben zur sicheren Seite, 0:00 als Latch, eine Quelle der Wahrheit für die Zahlen, Test je Regel in beide Richtungen.

## Regeltext (Game Manual v2.1, docs/ref/game_manual_v2.1.txt)
- **SG2a:** der Roboter "can never be larger than 24" wide or 24" long at any point during the Match", auch während vertikaler Ausfahrbewegungen (Bögen). Grenze einschließlich (24,00 ist erlaubt).
- **SG3:** kein Teil darf "50"" überschreiten, jederzeit. Einschließlich.
- **SG6:** höchstens ein Pin und ein Cup im Besitz. Besitz = "fully supported by the Robot" oder Objekt bewegt sich in allen Richtungen mit dem Roboter (Definition Possession, Anhang B). Bei Verstoß: sofort alles außer Abbauen stoppen.
- VAIRM1: VEX-U-Regeln gelten wie geschrieben, "except Robot sizes" (betrifft nur die Startgröße). 24"/50" gelten für Stacker und Runner gleich.
- **Matchende 0:00:** alle Motoren auf Halt ist eine Festlegung des Teams (CLAUDE.md, plan.md MID-05), kein Manual-Satz, der sich hier zitieren ließe.

## Entscheidung
- `lib/util/rule_guard.hpp`: reine Logik, kein dynamischer Speicher, keine PROS-Header. Bewusst kein Subsystem (kein request/update/Timeout-Vertrag) und ganz unten, weil der CycleController (C-01) und die Ausgabestufe des Steuer-Tasks beide nach unten aufrufen.
- Zahlen stehen einmal hier (`kMaxFootprintIn` usw.); `sim/rules.hpp` übernimmt sie, die Sim-Tests (SIM-05) bleiben unverändert grün.
- **Eingang 1** `may_grab(Grab, GuardInput)`: nur bei bekanntem Besitz und freiem Platz. Unbekannt (< 0) heißt nicht greifen.
- **Eingang 2** `filter(GuardCommand, GuardInput)` als letzter Filter vor der HAL:
  - Lift nach oben nur bei gehomtem Lift (`lift_ok`) und Lifthöhe < harte Grenze (50" − `lift_top_offset_in`). Nach unten bleibt immer erlaubt. Fehlt der Versatz (NaN, Standard), ist der Lift nach oben immer gesperrt.
  - Restzeit <= 0: alle Ausgaben 0 V mit Bremse, **gelatcht** (steigt die gemeldete Restzeit wieder, läuft nichts an). Unbekannte Restzeit (NaN) löst nichts aus.
  - Überbesitz (SG6) wird nur gemeldet; Abbauen ist Sache des Zyklus (Zustand AUSWERFEN).
  - Rückgabe `intervened` als Bitmaske (`kRuleSg2/Sg3/Sg6/MatchEnd`); die Simulation bildet sie auf `RULE:SG2` usw. ab. Kein `std::string` im Steuer-Task.
- Die harte Grenze sperrt nur die Spannung nach oben; die weiche Decke (`soft_lift_ceiling_in()` = hart − `soft_margin_in`) geht an `LiftSubsystem::set_ceiling_in`, damit der Lageregler vor der Sperre nach unten fährt und die Sperre nicht gegen ihn arbeitet.
- Der Guard ist in `src/main.cpp` angelegt (damit beide Roboter ihn mitkompilieren), aber noch nicht verdrahtet: Lift, Antrieb und Greifer haben noch keine Ports (TODO(HW)).

## Nicht Teil dieses Tickets
- **SG2 aus der Geometrie:** `rule_violations()` prüft gelieferte Maße; die Ausdehnung des Armes über dem Lift (Bögen) ist Hardware-Wissen je Roboter (TODO(HW)) und kommt mit der Messung.
- **Endgame-Höhe im Midfield** (plan.md: ab 10 s Restzeit, Roboter im Midfield): braucht den Midfield-Bereich (A-09, blockiert) und den Regelwert; eigenes Ticket M-08.
- **Autonomielinie:** gehört in den Planer.
- **Verhalten ohne Feldsteuerung** (Üben fährt nach 60 s nicht weiter?): Frage an einen Menschen; der Guard bekommt die Restzeit als Eingabe und entscheidet das nicht.

## Am Roboter zu prüfen
- Höhenversatz der höchsten Stelle über der gemessenen Lifthöhe (Anleitung in tasks/M-07.md).
- Footprint beim Ausfahren in allen Lift-/Armstellungen nachmessen (SG2).
