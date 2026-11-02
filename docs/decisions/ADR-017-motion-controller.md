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
