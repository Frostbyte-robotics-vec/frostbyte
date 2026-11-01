# ADR-008: Sensormodelle der Simulation

Status: angenommen (1. Nov 2026), Ticket SIM-03. Der Advisor (Opus) hat den Stand vor /finish geprüft; sein Hinweis, die Tests an eigene Werte statt an Schätzwerte zu binden und die Entscheidungen hier festzuhalten, ist eingearbeitet.

## Entscheidung
- **Ort.** `sim/sensor_models.hpp/.cpp` (nur Laptop). Klassen `OdometryModel` (Tracking-Rad beim Stacker, Antriebsencoder beim Runner), `ImuModel`, `GpsModel`. `load_sensor_params()` liest `roboter.<name>.sensoren` und `rauschen_profile.<profil>.skala` aus `sim_params.json`. Die Modelle setzen die Fake-Sensoren aus `hal/sim` nicht selbst; das macht die Welt, sobald das Fahrmodell (SIM-02) wahre Wege und Posen liefert.
- **Zufall.** Nur `sim::Rng`. `gauss()` ist die Summe von 12 Gleichverteilten minus 6 (Irwin-Hall, Sigma 1, Werte in [-6, 6]): nur Grundrechenarten, auf jeder Plattform bit-gleich. Box-Muller (log, cos) wäre es nicht garantiert. Jedes Modell zieht aus einem eigenen Strom (Seed und Kennzahl, über splitmix64 gemischt); die Ergebnisse hängen nicht von der Aufrufreihenfolge ab.
- **Skalierungsfehler und IMU-Drift** haben die Größe aus `sim_params.json`, das Vorzeichen wird je Lauf gezogen. So ist der Wert prüfbar, und die Läufe decken beide Richtungen ab.
- **Rauschprofil** (`none` 0, `realistic` 1, `harsh` 2) skaliert Skalierungsfehler, Rauschen, Drift, Aussetzeranteil und GPS-Sigma gemeinsam. `none` liefert perfekte Sensoren.
- **GPS.** Pro Aufruf immer gleich viele Zufallszahlen, damit ein Aussetzer die übrigen Messungen nicht verschiebt. Aussetzer sind unabhängig je Messung (keine Bündel). Gemeldete Güte `error_in = sigma_in * (1 + guete_verlust_pro_in_s * |v|)`; das Rauschen der Position folgt genau dieser Güte. Das ist unsere Auslegung von `guete_verlust_pro_in_s`, keine Messung.
- **Neuer Schlüssel** `gps.sigma_deg` (Rauschen der Blickrichtung) in `sim_params.json`, Quelle: Schätzung.

## Folgen
- Alle Zahlen sind Schätzungen; SIM-08 ersetzt sie nach echten Logs. Tests verwenden eigene Fixtures (`test/fixtures/sim_params_sensors.json`) bzw. eigene Parameter, nie die Schätzwerte (der Struktur-Test gegen die echte Datei prüft keine Werte).
- Am Roboter zu prüfen: Sigma von Tracking-Rad und Encoder, IMU-Drift je Minute, GPS-Sigma im Stand und in Fahrt, ob die Güte-Formel und unabhängige Einzel-Aussetzer zum echten GPS passen (echte Aussetzer könnten gehäuft auftreten).
