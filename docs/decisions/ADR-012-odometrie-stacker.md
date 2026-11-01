# ADR-012: Odometrie Stacker (Tracking-Räder + IMU)

Status: angenommen (1. Nov 2026), Ticket D-02. Der Advisor (Opus) hat vorgegeben: Test-Sensorwerte aus den Seitenwegen des Fahrmodells ableiten (nicht aus der wahren Pose), Räder mit Versatz testen, Fehler über den ganzen Lauf prüfen.

## Entscheidung
- `lib/estimation/odometry.hpp`, Klasse `TrackingOdometry`, reine Logik ohne PROS und ohne `config/`. Die Radlage (`TrackingGeometry`) kommt über den Konstruktor; die echten Werte stehen später in `config/stacker.hpp` (TODO(HW)).
- Eingabe sind Wegsummen beider Räder (Zoll) und das IMU-Heading (Grad); die Pose (`Pose`, Zoll/Grad, 0° = +x, gegen den Uhrzeigersinn positiv) ist die Ausgabe. Das Heading kommt allein vom IMU; nur dessen Änderung zählt, ein Einbauversatz fällt über `reset()` heraus.
- Pro Takt: Radversatz abziehen (`dx = Δparallel + dθ·y_w`, `dy = Δquer − dθ·x_w`), dann Kreisbogen (Pose-Exponential). Das IMU-Vorzeichen muss vor dem Aufruf stimmen (HAL/config).
- Nicht Teil von D-02: Umschalten auf Antriebsencoder bei Ausfall eines Rads oder des IMU (D-03).

## Folgen
- D-03 (Runner) kann `Pose` wiederverwenden; die Schicht darüber (Bewegung zu Pose) liest nur `pose()`.
