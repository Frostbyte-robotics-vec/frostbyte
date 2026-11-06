# ADR-039: Rückmeldung an den Fahrer (Controller-Anzeige und Vibration)

Status: angenommen (C-04, 2026-11-06)

## Entscheidung
- Neue HAL-Schnittstelle `hal/interface/controller_output.hpp` (`rumble`, `print`, beide mit Rückgabewert „angenommen“), PROS-Seite `hal/pros/pros_controller_output.hpp`, Testdouble `hal/sim/sim_controller_output.hpp`. Kein PROS-Header in `lib/`.
- Die Logik steht in `lib/match/controller_feedback.hpp` (`ControllerFeedback`): Vibration über das vorhandene `endgame_rumble` (ADR-014), bei 15 s kurz (`.`), bei 10 s lang (`-`); ein Sprung über beide Schwellen vibriert einmal (niedrigere Schwelle). Die erste Zeit nach `reset()` löst nie aus (keine Vorgängerzeit).
- Zeile 0 zeigt `<Restzeit>s <Status>` (aufgerundet), immer auf 15 Zeichen mit Leerzeichen aufgefüllt (`set_text` überschreibt nur die gesendeten Zeichen, sonst bliebe bei `10s` → `9s` ein Rest stehen), gesendet nur bei Änderung und höchstens alle 100 ms (PROS: 50 ms Mindestabstand). Lehnt der Controller ab, bleibt der Text „unbestätigt“ und wird im nächsten Zyklus erneut gesendet.
- Höchstens ein Controllerbefehl je `update()`: Bei der Vibration folgt der Text im nächsten Zyklus. Die Vibration ist Best Effort und wird nicht wiederholt (Rückgabewert ignoriert); nur die Textzeile wird bei Ablehnung erneut gesendet.
- Im Prüfprogramm (`port_check_enabled`, F-03) läuft die Rückmeldung nicht: `opcontrol()` verhält sich dort wie vorher.
- Aufruf in `opcontrol()` mit 100 ms (`task_delay_until`), nie im Steuer-Task: Controllerbefehle blockieren.

## Annahmen (TODO(HW), Ticket C-09)
- Der Fahrerlauf dauert 60 s (Skills; Seitenzahl im Game Manual nicht geprüft) und beginnt mit `opcontrol()`; ob das am Feld mit dem Spielstart zusammenfällt, ist ungeprüft. Eine echte Match-Uhr hat PROS nicht.
- Muster `.` und `-` sind am Controller unterscheidbar; die Textzeile flackert nicht.
