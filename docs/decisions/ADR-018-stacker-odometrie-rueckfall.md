# ADR-018: Stacker-Odometrie mit Rückfall bei Ausfall eines Tracking-Rads

Status: angenommen (2. Nov 2026), Ticket D-08. Der Advisor (Opus) hat vorgegeben: eine aktive Pose, die andere Odometrie wird jeden Takt darauf gesetzt (kein Posensprung per Konstruktion); erst prüfen, dann integrieren, auch in der Wartezeit des Stillstands; `DriveOdometry` nicht per `reset()` angleichen (würde den IMU-Befund löschen); IMU nicht ungeprüft in `TrackingOdometry`.

## Entscheidung
- `lib/estimation/stacker_odometry.hpp`, Klasse `StackerOdometry`, enthält `TrackingOdometry` (D-02) und `DriveOdometry` (ADR-015). Eingabe: Summen links/rechts, Parallel- und Querrad je mit Gültig-Flag, IMU-Heading mit Gültig-Flag.
- Die Klasse besitzt die aktive Pose. Jeden Takt rechnet die Schatten-`DriveOdometry` von der aktiven Pose aus (ihre Drehung dieses Takts ist der Erwartungswert), danach wird entschieden, wer die Pose liefert; die andere wird auf sie gesetzt (`DriveOdometry::set_pose`, neu, ändert nur die Pose; `TrackingOdometry::reset` mit den aktuellen Rohwerten).
- Erwartungswert je Takt aus den Antriebsencodern und der Drehung der Schattenodometrie (IMU oder Encoder):
  - Parallelrad: `(Δlinks + Δrechts)/2 − Δθ · parallel_y_in`
  - Querrad: `Δθ · perpendicular_x_in` (der Antrieb hat keine Seitenbewegung)
- Ausfallarten je Rad: **ungültig** (HAL-Flag oder nicht endlich) und **Sprung** (|Messung − Erwartung| > `jump_in` in einem Takt) schalten sofort um; **Stillstand** (|Erwartung| > `still_drive_in`, Messung < `still_wheel_in`) nach `still_cycles` Takten in Folge. Zusätzlich IMU-Ausfall: sobald die Schattenodometrie `degraded()` ist, gilt der Rückfall (Grund `imu`), damit kein NaN-Heading in die Tracking-Pose gelangt.
- Der Takt, der den Ausfall zeigt, und alle Verdachtstakte (Stillstandszähler > 0, `DriveOdometry::imu_suspect()`) werden mit den Antriebsencodern gerechnet; danach wird die Tracking-Seite wieder angeschlossen. Sonst bliebe die Pose um `still_cycles` × Weg je Takt zurück.
- Der Rückfall ist endgültig bis `reset()`. `degraded()` und `fallback_reason()` (`StackerFallback`) wie in ADR-015. Verdrahtung in Safe-Modus, Log und Controller-Warnung gehört nicht zu D-08.
- Schwellen kommen über den Konstruktor; echte Werte später in `config/` (TODO(HW)). Tests nehmen eigene Werte.

## Grenzen
- Querrad: Die Antriebsencoder liefern keine Seitenreferenz. Ein eingefrorenes Querrad ist nur bei Drehung mit `perpendicular_x_in ≠ 0` sichtbar und nur, wenn der Erwartungswert über `still_drive_in` liegt (im Anlauf einer Drehung nicht); bei Geradeausfahrt liest es ohnehin ≈ 0 und ist harmlos. Je Takt geht höchstens `still_drive_in` als Fehler ein.
- Schiebt ein Gegner den Roboter seitlich oder rutscht er, kann das Querrad mehr als `jump_in` messen: falscher Alarm → Rückfall (sicher, aber ungenauer). Ebenso drehende Antriebsräder gegen eine Wand (Parallelrad steht, Encoder drehen). `jump_in` und `still_cycles` am Roboter so wählen, dass Schlupf darunter bleibt.
- Rückfall rechnet ohne Querrad (`dy = 0`) und mit Antriebsencodern: ungenauer als Tracking, aber stabil.
- Ausfall der Antriebsencoder selbst wird nicht geprüft.
