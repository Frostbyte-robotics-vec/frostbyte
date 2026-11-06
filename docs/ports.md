# Port-Tabelle

Gerät, Port, Drehrichtung und Getriebeeinsatz beider Roboter (F-07). Die Werte stehen in `config/stacker.hpp` und `config/runner.hpp`
(`port_expect_table`); ein Test (`test/test_port_table.cpp`) prüft, dass Tabelle und config übereinstimmen, dass kein Port doppelt
vorkommt und dass keine Ports außerhalb von `config/` stehen.

**Stand:** Es gibt noch keine belegten Werte. Das CAD liefert keine Ports, und am Roboter wurde noch nichts bestätigt. Alles steht
auf `TODO(HW)`; geraten wird nichts. Die Werte trägt F-09 nach dem Bench-Test mit `port_check` (F-03) ein.

Spalten: **Port** 1 bis 21 (`TODO(HW)` = offen). **Drehrichtung** ist die Richtung, in die der Motor mit positiver Spannung die
Mechanik bewegt (`Bewegung` in `port_check`); ein umgekehrter Motor steht in config/ mit negativem Port. **Getriebe** ist die
Patrone (rot 100 / grün 200 / blau 600 RPM). **Quelle** nennt, woher der Wert stammt (CAD, Bench-Test) oder was offen ist.

| Roboter | Gerät | Art | Port | Drehrichtung | Getriebe | Quelle |
|---|---|---|---|---|---|---|
| stacker | Antrieb L | Motor | TODO(HW) | positiv = Rad vorwaerts | TODO(HW) | offen (F-09): Antrieb links; Anzahl Motoren je Seite offen |
| stacker | Antrieb R | Motor | TODO(HW) | positiv = Rad vorwaerts | TODO(HW) | offen (F-09): Antrieb rechts; Anzahl Motoren je Seite offen |
| stacker | Track par | Rotation | TODO(HW) | – | – | offen (F-09): Tracking-Rad parallel (Odometrie) |
| stacker | Track quer | Rotation | TODO(HW) | – | – | offen (F-09): Tracking-Rad quer (Odometrie) |
| stacker | IMU | IMU | TODO(HW) | – | – | offen (F-09): Inertialsensor |
| stacker | GPS | GPS | TODO(HW) | – | – | offen (F-09): GPS-Sensor |
| stacker | Intake | Motor | TODO(HW) | positiv = Rollen einziehen | TODO(HW) | offen (F-09): Intake |
| stacker | Claw | Motor | TODO(HW) | positiv = Claw zu | TODO(HW) | offen (F-09): Greifer; Runner: Vorhandensein offen |
| stacker | Lift | Motor | TODO(HW) | positiv = Lift hoch | TODO(HW) | offen (F-09): Lift; Runner: Vorhandensein offen |
| stacker | Lift Sensor | Rotation | TODO(HW) | – | – | offen (F-09): Lift-Positionssensor (ADR-019: Rotation oder Motorencoder offen) |
| stacker | Arm | Motor | TODO(HW) | positiv = Arm Winkel steigt | TODO(HW) | offen (F-09): Arm (RuleGuard, M-09); Vorhandensein offen |
| runner | Antrieb L | Motor | TODO(HW) | positiv = Rad vorwaerts | TODO(HW) | offen (F-09): Antrieb links; Anzahl Motoren je Seite offen |
| runner | Antrieb R | Motor | TODO(HW) | positiv = Rad vorwaerts | TODO(HW) | offen (F-09): Antrieb rechts; Anzahl Motoren je Seite offen |
| runner | Track par | Rotation | TODO(HW) | – | – | offen (F-09): Tracking-Rad parallel (Odometrie) |
| runner | Track quer | Rotation | TODO(HW) | – | – | offen (F-09): Tracking-Rad quer (Odometrie) |
| runner | IMU | IMU | TODO(HW) | – | – | offen (F-09): Inertialsensor |
| runner | GPS | GPS | TODO(HW) | – | – | offen (F-09): GPS-Sensor |
| runner | Intake | Motor | TODO(HW) | positiv = Rollen einziehen | TODO(HW) | offen (F-09): Intake |
| runner | Claw | Motor | TODO(HW) | positiv = Claw zu | TODO(HW) | offen (F-09): Greifer; Runner: Vorhandensein offen |
| runner | Lift | Motor | TODO(HW) | positiv = Lift hoch | TODO(HW) | offen (F-09): Lift; Runner: Vorhandensein offen |
| runner | Lift Sensor | Rotation | TODO(HW) | – | – | offen (F-09): Lift-Positionssensor (ADR-019: Rotation oder Motorencoder offen) |
| runner | Arm | Motor | TODO(HW) | positiv = Arm Winkel steigt | TODO(HW) | offen (F-09): Arm (RuleGuard, M-09); Vorhandensein offen |
