# ADR-027: RuleGuard (lib/util/rule_guard.hpp)

Status: angenommen (3. Nov 2026), Ticket M-07. Der Advisor (Opus) hat vorgegeben: Regeltext vorher lesen, reiner Filter ganz unten wie GoalBook, zwei Eingänge (Frage "darf ich greifen?" und Ausgabefilter vor der HAL), unbekannte Eingaben zur sicheren Seite, 0:00 als Latch, eine Quelle der Wahrheit für die Zahlen, Test je Regel in beide Richtungen.

## Regeltext (Game Manual v2.1, docs/ref/game_manual_v2.1.txt)
- **SG2a:** der Roboter "can never be larger than 24" wide or 24" long at any point during the Match", auch während vertikaler Ausfahrbewegungen (Bögen). Grenze einschließlich (24,00 ist erlaubt).
- **SG3:** "no part of the Robot may exceed an overall height of 50"" jederzeit (Webversion v2.1 gelesen; die lokale Textkopie ist am rechten Rand abgeschnitten). Einschließlich. Der Changelog-Eintrag "SG3 clarify Endgame expansion limits" ändert den Wert nicht.
- **SG12 (Endgame):** in v2.1 kein Höhenwert; laut Changelog wurde die frühere harte Grenze zu einer weichen mit mündlichen Verwarnungen. Eine harte Endgame-Höhe im Midfield (plan.md) ist daher eine Team-Entscheidung, keine v2.1-Regel (siehe M-08).
- **SG6:** höchstens ein Pin und ein Cup im Besitz. Besitz = "fully supported by the Robot" oder Objekt bewegt sich in allen Richtungen mit dem Roboter (Definition Possession, Anhang B). Bei Verstoß: sofort alles außer Abbauen stoppen.
- **VUR1b/c (VEX U):** ein Roboter höchstens 24" × 24" × 24", der andere 15" × 15" × 15", jeweils **zu Beginn des Matches**. Das ist nur die Startgröße; SG2/SG3 (24"/50") gelten danach für Stacker und Runner gleich. `kMaxFootprintIn` bleibt für beide 24; `config::*::footprint_in` (15 beim Runner) ist die Startgröße und wird vom Guard nicht benutzt. (VAIRM1 im Abschnitt VEX AI sagt Ähnliches, ist hier nicht die Grundlage.)
- **Matchende 0:00:** alle Motoren auf Halt ist eine Festlegung des Teams (CLAUDE.md, plan.md MID-05), kein Manual-Satz, der sich hier zitieren ließe.

## Entscheidung
- `lib/util/rule_guard.hpp`: reine Logik, kein dynamischer Speicher, keine PROS-Header. Bewusst kein Subsystem (kein request/update/Timeout-Vertrag) und ganz unten, weil der CycleController (C-01) und die Ausgabestufe des Steuer-Tasks beide nach unten aufrufen.
- Zahlen und Vergleiche stehen einmal hier (`kMaxFootprintIn`, `rule_violations()`); `sim/rules.cpp` ruft `rule_violations()` und übersetzt die Bits in seine Texte (`RULE:SG2` usw.). Die Sim-Tests (SIM-05) bleiben unverändert grün.
- **Eingang 1** `may_grab(Grab, GuardInput)`: nur bei bekanntem Besitz und freiem Platz. Unbekannt (< 0) heißt nicht greifen. SG6 wird nur hierüber erzwungen: der Ausgabefilter schaltet Greifer und Intake nicht ab. C-01 und M-04 müssen `may_grab` aufrufen.
- **Eingang 2** `filter(GuardCommand, GuardInput)` als letzter Filter vor der HAL:
  - Lift nach oben nur bei gehomtem Lift (`lift_ok`) und Lifthöhe < harte Grenze (50" − `lift_top_offset_in`). Nach unten bleibt immer erlaubt. Fehlt der Versatz (NaN, Standard), ist der Lift nach oben immer gesperrt.
  - Restzeit <= 0: alle Ausgaben 0 V mit Bremse, **gelatcht** (steigt die gemeldete Restzeit wieder, läuft nichts an). Unbekannte Restzeit (NaN) löst nichts aus.
  - Disabled (`GuardInput.disabled`): alle Ausgaben 0 V mit Bremse, **ohne** Latch (nach Enable läuft es wieder). Erfüllt "Disabled → Motoren Halt" aus F-04.
  - Überbesitz (SG6) wird nur gemeldet; Abbauen ist Sache des Zyklus (Zustand AUSWERFEN).
  - Rückgabe `intervened` als Bitmaske (`kRuleSg2/Sg3/Sg6/MatchEnd`); die Simulation bildet sie auf `RULE:SG2` usw. ab. Kein `std::string` im Steuer-Task.
- Die harte Grenze sperrt nur die Spannung nach oben; die weiche Decke (`soft_lift_ceiling_in()` = hart − `soft_margin_in`) geht an `LiftSubsystem::set_ceiling_in`, damit der Lageregler vor der Sperre nach unten fährt und die Sperre nicht gegen ihn arbeitet.
- Der Guard ist in `src/main.cpp` angelegt (damit beide Roboter ihn mitkompilieren), aber noch nicht verdrahtet: Lift, Antrieb und Greifer haben noch keine Ports (TODO(HW)).

## Nicht Teil dieses Tickets
- **SG2/SG3 aus der Geometrie:** `rule_violations()` prüft gelieferte Maße, der Filter erzwingt SG2 noch nicht (Arm- und Lift-Ausladung sind Hardware-Wissen je Roboter): Ticket M-09.
- **Endgame-Höhe im Midfield** (plan.md: ab 10 s Restzeit, Roboter im Midfield): braucht den Midfield-Bereich (A-09, blockiert) und den Regelwert; eigenes Ticket M-08 (zuerst klären, ob überhaupt ein harter Wert gewollt ist).
- **Autonomielinie:** gehört in den Planer.
- **Verhalten ohne Feldsteuerung** (Üben fährt nach 60 s nicht weiter?): Frage an einen Menschen; der Guard bekommt die Restzeit als Eingabe und entscheidet das nicht.

## Am Roboter zu prüfen
- Nichts in M-07 selbst; die Messungen (Höhenversatz, Footprint beim Ausfahren) stehen in M-09.

## Nachtrag M-09: SG2/SG3 aus der Geometrie (Software-Teil)
Der Advisor (Opus) hat vorgegeben: dieselbe Form wie beim Lift-SG3, Arm-Befehle sind Teil des Guards, unbekannt oder nicht gemessen heißt sperren.
- `GuardCommand` bekommt `arm_v` **hinter** `brake` (bestehende Initialisierungen mit vier Werten bleiben gültig); alle Halt-Befehle im Filter sind jetzt mit benannten Feldern gebaut, damit ein neues Feld den Halt nie still verfälscht. `arm_v != 0` zählt bei Disabled und Matchende als Eingriff; der Arm steht dann auch (inkl. Latch bei 0:00).
- `GuardInput` bekommt `arm_ok` und `arm_deg` (NaN = unbekannt). Konvention: **positive `arm_v` erhöht `arm_deg`**; die physische Richtung liegt in HAL/config (TODO(HW)), der Guard kennt sie nicht.
- **Arm-Grenzen** `arm_min_deg` .. `arm_max_deg` (Bereich, in dem Footprint <= 24" und Höhe <= 50" bleiben): bei oder jenseits einer Grenze wird nur die Richtung nach außen gesperrt, zurück in den Bereich bleibt frei (wie Lift nach unten). Unbekannter Armzustand (`!arm_ok`, NaN-Winkel) oder nicht gemessene Grenzen (NaN, Standard) sperren beide Richtungen. Gemeldet werden `kRuleSg2 | kRuleSg3`, weil der Bereich beide Regeln zusammen abdeckt.
- **Lift-SG2:** neue Größe `lift_footprint_ceiling_in` (Lifthöhe, ab der der Footprint 24" überschreitet). `hard_lift_ceiling_in()` ist jetzt das Minimum aus SG3-Decke (`sg3_lift_ceiling_in()`) und SG2-Decke (`sg2_lift_ceiling_in()`), also folgt `soft_lift_ceiling_in()` automatisch; sonst arbeitete die Sperre gegen den Lageregler des Lifts. Gemeldet wird die Regel, deren Decke erreicht ist (beide, wenn beide).
- **NaN gegen +inf:** NaN = nicht gemessen = Lift nach oben gesperrt (Standard). `+inf` = gemessen, der Lift verlässt den Footprint nie. (Deshalb `isnan` statt `isfinite`.) Die Testkonfigurationen setzen `+inf`; `config::*::rule_guard` bleibt bis zur Messung (M-11) NaN, der Lift also nach oben gesperrt.
- **Grenzen des Modells:** getrennte Grenzen je Achse sind nur richtig, wenn jede der Worst Case über die andere Achse ist: Armbereich bei ungünstigster Lifthöhe gemessen, `lift_top_offset_in` mit dem Arm an seinem höchsten Punkt. Zeigt die Messung (M-11), dass das zu streng wird, kommt eine Kurve (MonotoneTable) statt der Zahlen.
- **Simulation unverändert:** `sim/` baut keinen `RuleGuard`, es benutzt nur `rule_violations()` und die Konstanten. Kein Einfluss auf A-04 und die Referenztests.
- Der Runner hat keine eigene Größengrenze nach dem Start (VUR1b/c, siehe oben).
