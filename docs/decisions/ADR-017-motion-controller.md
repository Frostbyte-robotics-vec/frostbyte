# ADR-017: MotionController selbst, LemLib nur als PID hinter einer Regler-Schnittstelle

Status: angenommen (2. Nov 2026), Ticket D-05. Nummer 017, weil 013–016 auf offenen Branches belegt sind
(S-07: 013, C-03: 014, D-03: 015, D-04: 016).

## Entscheidung
- Bewegungen (`turn_to`, `move_to`, `follow` mit Pure Pursuit) rechnet ein eigener `frostbyte::MotionController`
  in `lib/control/motion_controller.hpp` (Schicht 4). Derselbe Code läuft auf dem Roboter und in der Simulation (ADR-001).
- Schnittstelle wie ein Subsystem (ADR-011): `request(MotionCommand, now_s)` → bool, `state()`, `update(now_s, pose)` →
  Spannung links/rechts. Zustände `idle`, `running`, `done`, `timeout`, `blocked`; jeder Befehl endet in genau einem
  Endzustand, dort 0 V. Ein Befehl ohne endliches Timeout > 0, mit nicht endlichem Ziel oder mit leerem, übervollem
  oder ungültigem Pfad wird abgelehnt (false, Zustand `idle`, 0 V).
- `blocked` = im ganzen Prüffenster (0,5 s) liegt mindestens `blocked_min_effort_v` an, und der Roboter fährt weniger
  als `blocked_min_motion_in` und dreht weniger als `blocked_min_motion_deg`. Nur „Spannung ohne Bewegung“ zählt;
  langsames Annähern ans Ziel (kleine Spannung) und träges Anfahren lösen es nicht aus.
- `move_to`: Fährt der Roboter nach einer Annäherung (näher als `near_in`) über das Ziel hinaus, setzt er zurück,
  statt zu wenden (Lenkfehler auf −90…90° gefaltet). Ohne diese Regel kreiste er in der Simulation ums Ziel.
- Pfade haben eine feste Kapazität (`Path::capacity = 32`), kein dynamischer Speicher.
- Ungültige Pose (NaN/inf, z. B. Sensorausfall): 0 V, und nach dem Ausfall beginnen Blocked-Fenster, Beruhigungszeit
  und dt der Regler neu (Hinweis aus dem KI-Review von PR #28). Das Timeout läuft weiter.
- Einschränkung `follow`: Der Zielpunkt ist der weiteste Schnitt des Vorausschau-Kreises über alle restlichen Abschnitte.
  Läuft ein Pfad innerhalb des Vorausschau-Radius zurück (Haarnadel, enge Schleife), wird er abgekürzt. Solche Pfade
  in einzelne `follow`-/`move_to`-Befehle teilen.
- Regler: eigener `frostbyte::Pid` (`lib/control/pid.hpp`) mit Ausgabegrenze, I-Grenze, i_zone und Anti-Windup
  (keine Integration, solange die Ausgabe in Fehlerrichtung gesättigt ist). Der Controller kennt nur die Schnittstelle
  `frostbyte::Regulator`.
- LemLib-Adapter: `src/lemlib_regulator.hpp` packt `lemlib::PID` hinter `Regulator`. Er liegt in `src/`, weil LemLib-Header
  nur in `hal/pros/` und `src/` stehen dürfen (ADR-003) und ein Regler nicht zur Hardware-Schicht gehört. `lemlib::PID`
  rechnet je Aufruf statt je Sekunde (pid.cpp.o in firmware/LemLib.a ruft weder millis noch micros auf, geprüft mit
  `arm-none-eabi-nm -A firmware/LemLib.a`); der Adapter rechnet ki · T und kd / T mit der festen Taktzeit um.
  Standard ist der eigene Pid (`config::motion_uses_lemlib_pid = false`); der Schalter dient dem Vergleich in D-07.
- Sim-Adapter: `RobotSetup::closed_loop` in `sim/world.hpp` wird je Steuertakt mit der geschätzten Pose (Sensormodelle
  und Odometrie der Simulation, nicht die Wahrheit) aufgerufen; `sim/motion_adapter.hpp` (`motion_loop`) verbindet
  den MotionController damit. Der Steuerblock in `run_match` läuft jetzt vor den Befehlen im selben ms; er ändert weder
  Spannungen der Handsteuerung noch Ereignisse, darum sind alle bisherigen Logs byte-gleich (geprüft mit den
  Skills-Routen, Seeds 1–3, und `--demo`, Seeds 1–2).
- Startwerte (Toleranzen, Verstärkungen, Spurbreite) stehen je Roboter in `config/stacker.hpp` und `config/runner.hpp`,
  Verstärkungen und Spurbreite als TODO(HW) für D-07.

## Warum nicht lemlib::Chassis
- `turnToHeading`, `moveToPoint`, `follow` usw. laufen standardmäßig asynchron (`async = true` in
  `include/lemlib/chassis/chassis.hpp`), also in eigenen Tasks, und `calibrate()` startet die LemLib-Odometrie als
  eigenen Task. Das widerspricht „ein Steuer-Task, 10 ms“ (CLAUDE.md).
- Chassis schreibt die Motoren selbst. Der RuleGuard muss aber jede Ausgabe vor den Motoren sehen (Halt bei 0:00, M-07).
- `lemlib::ExitCondition` misst Zeit mit `pros::millis()`; am Laptop und in der Simulation ist das nicht deterministisch.

## Rat des Advisors (Opus)
Eigener Controller überall ist richtig (gleicher Code in Robot und Sim). Wichtig: ADR-Nummer gegen offene Branches
prüfen; LemLib-Adapter muss im Roboter-Build wirklich linken (geprüft: mit `motion_uses_lemlib_pid = true` ist
`lemlib::PID::update` im ELF); Sim-Haken darf ohne Nutzung kein Byte im Log ändern; Tests für falsches BLOCKED,
±180°, ungültige Timeouts und übervolle Pfade.

## Folgen
- Drive (Schicht 3, „Bewegung zu Pose“) bekommt die Pose als zweiten Parameter von `update()` (ADR-011), sobald
  Routen (A-03) den Controller im Steuer-Task nutzen. Bis dahin sind die Objekte in `src/main.cpp` angelegt, aber
  ungenutzt.
- D-06 (Tuning-Modus) ändert `PidGains` zur Laufzeit; D-07 tunt am Testchassis und vergleicht beide Regler.

## Nachtrag D-12: engere Toleranzen für die Antriebs-Prüfung
- Entscheidung: Weg 1, global enger. `move_tol_in` 1.0 → 0.5 (1,27 cm), `turn_tol_deg` 2.0 → 1.5 in `config/stacker.hpp` und
  `config/runner.hpp`. Die Grenzen der Prüfung (2 cm, 2°) lassen so Reserve für den Unterschied zwischen geschätzter und
  wahrer Pose: der Regler stoppt auf der Schätzung, `drive_check` misst die Wahrheit.
- Warum nicht Weg 2 (`tol=` bei `goto pose`): `goto <NAME> tol=` wird zwar geparst, aber nirgends an den Controller
  weitergereicht (`MotionCommand::move_to` kennt keine Toleranz). Weg 2 wäre eine Schnittstellenänderung in Controller,
  beiden Ausführern und Parser. Das ist ein eigenes Ticket (D-14); die Prüfroute misst so dasselbe Verhalten wie die Skills-Routen.
- Warum Drehtoleranz nur 1,5° und nicht 1,0°: Mit 1,0° braucht jede Drehung in der Simulation etwa 1,5 s; die 40 Drehungen von
  `drive_turns` waren dann bei Matchende (60 s) noch nicht durch (Runner endete mitten in einer Drehung bei −149°). Mit 1,5° endet
  die Route rechtzeitig.
- Nachweis in der Simulation (ungetunte Regler, Seeds 1–5, Stacker allein, der Runner fährt `empty.route`, Prüfrouten mit
  `--field test/fixtures/field_drive.json`). Der Runner ist hier nicht vergleichbar: `field_drive.json` startet ihn bei
  (39,37 / 40 / 180°), er fährt also nicht 2 m geradeaus, und „3420 statt 3600 Grad“ bei `drive_turns` ist die erste Drehung im
  Uhrzeigersinn von 180° aus. Runner-Zahlen stehen deshalb nicht im Nachtrag; D-10 misst ihn am Roboter.
  - Matchzeit, zu der der Routenplan leer ist (`route_step` leer), Skills-Routen, Mittel der Seeds 1–5: Stacker 54,6 s
    (Toleranzen 1,0/2,0) → 55,2 s (0,5/1,5); Runner 56,3 s → 57,6 s, davon 3 von 5 Seeds bis 58,0 s, also bis zum Timeout
    des letzten `goto MIDFIELD_R` (`at 52s`, `timeout=6.0`). Das Ausrollen kostet in der ungetunten Simulation etwa 1 s je Route.
  - Bewegungs-Timeouts schreiben kein `TIMEOUT:` ins Log (nur die Ereigniszähler 0 vorher wie nachher); sichtbar sind sie an der Endzeit.
    `PLACED:` ist in Skills-Routen vorher und nachher 0 (Platzhalter-Koordinaten), die Zahl taugt hier nicht als Vergleich.
  - `drive_straight` Stacker, wahre Pose: Toleranz 1,0 → 6,05 / 6,01 / 6,03 / 3,89 / 6,04 cm (alle 5 Seeds nicht bestanden);
    0,5 → 2,20 / 2,00 / 1,67 / 0,10 / 1,75 cm (3 von 5 bestanden); 0,6 → 2,2 bis 2,6 cm in 4 von 5 Seeds; 0,4 → fast alle
    Läufe laufen bis ins `goto`-Timeout von 8 s, deshalb verworfen.
  - **Kriterium nicht voll erfüllt:** Der Sim-Lauf von `drive_straight` endet bei 0,5 in 2 von 5 Seeds mit 2,00 bzw. 2,20 cm,
    also nicht unter 2,0 cm. Ursache ist der Schätzfehler der ungetunten Simulation (Odometrie weicht 0,5 bis 3,3 cm von der
    wahren Pose ab), nicht die Toleranz; eine engere Toleranz verschiebt das Problem in Timeouts. Die Bestehensfrage klärt D-10 mit
    getunten Reglern am Roboter. `drive_turns` Stacker: Endrichtung 0,56 bis 2,38° (2 von 5 Seeds über 2°), die Route endet bei
    ca. 51 s; dort bleibt „Drehrichtung wechselt“ wie vorher im Bericht, das ist Reglergüte.
- Absicherung: Test in `test/test_motion_controller.cpp` verlangt `move_tol_in·2,54 ≤ 1,5 cm` und `turn_tol_deg ≤ 1,5°` in beiden Configs.
- Rat des Advisors (Opus): Messung je Roboter einzeln, Reserve für Schätzfehler (0,4–0,5″ / 0,75–1,0°), Timeouts statt nur
  Laufzeit zählen (wegen `on_timeout=skip`), Config-Test, totes `tol=` als eigenes Ticket.
