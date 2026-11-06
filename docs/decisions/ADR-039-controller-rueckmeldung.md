# ADR-039: Rückmeldung an den Fahrer (Controller-Anzeige und Vibration)

Status: angenommen (C-04, 2026-11-06)

## Entscheidung
- Neue HAL-Schnittstelle `hal/interface/controller_output.hpp` (`rumble`, `print`, beide mit Rückgabewert „angenommen“), PROS-Seite `hal/pros/pros_controller_output.hpp`, Testdouble `hal/sim/sim_controller_output.hpp`. Kein PROS-Header in `lib/`.
- Die Logik steht in `lib/match/controller_feedback.hpp` (`ControllerFeedback`): Vibration über das vorhandene `endgame_rumble` (ADR-014), bei 15 s kurz (`.`), bei 10 s lang (`-`); ein Sprung über beide Schwellen vibriert einmal (niedrigere Schwelle). Die erste Zeit nach `reset()` löst nie aus (keine Vorgängerzeit).
- Zeile 0 zeigt `<Restzeit>s <Status>` (aufgerundet, höchstens 15 Zeichen), gesendet nur bei Änderung und höchstens alle 100 ms (PROS: 50 ms Mindestabstand). Lehnt der Controller ab, bleibt der Text „unbestätigt“ und wird im nächsten Zyklus erneut gesendet.
- Aufruf in `opcontrol()` mit 100 ms (`task_delay_until`), nie im Steuer-Task: Controllerbefehle blockieren.

## Annahmen (TODO(HW), Ticket C-09)
- Der Fahrerlauf dauert 60 s (Skills) und beginnt mit `opcontrol()`; ob das am Feld mit dem Spielstart zusammenfällt, ist ungeprüft. Eine echte Match-Uhr hat PROS nicht.
- Muster `.` und `-` sind am Controller unterscheidbar; die Textzeile flackert nicht.
