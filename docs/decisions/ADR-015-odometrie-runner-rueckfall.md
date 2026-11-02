# ADR-015: Odometrie Runner und Rückfall bei IMU-Ausfall

Status: angenommen (2. Nov 2026), Ticket D-03. Der Advisor (Opus) hat vorgegeben: erst prüfen, dann integrieren; verdächtige Takte schon mit der Encoder-Drehung rechnen; Zuwächse statt Summen; Test auf falschen Alarm, sonst besteht auch eine Odometrie, die immer im Rückfall läuft.

## Entscheidung
- `lib/estimation/drive_odometry.hpp`, Klasse `DriveOdometry`, reine Logik ohne PROS und ohne `config/`. Eingabe: Wegsummen von linker und rechter Antriebsseite (Zoll), IMU-Heading (Grad) und ein Gültig-Flag der HAL. Ausgabe: `Pose` wie in ADR-012.
- Position aus dem mittleren Seitenweg als Kreisbogen. Ohne Querrad gibt es keine Seitenbewegung (`dy = 0`).
- `Pose` und der Bogenschritt (`advance_pose`) liegen in `lib/estimation/pose.hpp` und werden von `TrackingOdometry` (D-02) und `DriveOdometry` gemeinsam genutzt.
- Heading im Normalbetrieb aus der IMU-Änderung, im Rückfall aus `(Δrechts − Δlinks) / track_width_in`.
- Ausfall des IMU (vor dem Integrieren geprüft, damit der auffällige Takt nicht in die Pose eingeht):
  - **ungültig:** HAL meldet keine gültige Messung oder der Wert ist nicht endlich → sofort Rückfall (`imu_invalid`).
  - **Sprung:** IMU-Drehung und Encoder-Drehung weichen in einem Takt um mehr als `jump_deg` ab → Rückfall (`imu_jump`). Der Winkelsprung ±180° zählt nicht, beide Drehungen werden vorher normiert.
  - **eingefroren:** IMU-Drehung < `still_imu_deg` bei Encoder-Drehung > `still_encoder_deg` über `still_cycles` Takte in Folge → Rückfall (`imu_frozen`). Schon die verdächtigen Takte rechnen mit der Encoder-Drehung, damit das Heading in der Wartezeit nicht wegläuft. Ein eingefrorenes IMU bei Geradeausfahrt bleibt unentdeckt und ist dort auch harmlos.
- Der Rückfall ist endgültig bis zum nächsten `reset()` (neues Match). `degraded()` und `fallback_reason()` sind für die Schicht darüber (Safe-Modus, Log, Controller-Warnung); diese Verdrahtung gehört nicht zu D-03.
- Schwellen und Spurbreite kommen über den Konstruktor. Die echten Werte stehen später in `config/` (TODO(HW)); Tests nehmen eigene Werte.

## Folgen
- Rückfall bei Ausfall eines Tracking-Rads (Stacker) ist hier noch nicht gebaut, nur der Baustein: `DriveOdometry` ist die Stacker-Rückfallodometrie. Die Prüfung der Tracking-Räder gegen die Antriebsencoder und die Umschaltung `TrackingOdometry` → `DriveOdometry` bekommt Ticket D-08.
- Bekannte Grenze: schiebt der Roboter gegen eine Wand und die Räder drehen durch, zeigen die Encoder Drehung bei stehendem IMU. Nach `still_cycles` Takten wird dann fälschlich umgeschaltet. Die Taktzahl am Roboter hoch genug wählen.
