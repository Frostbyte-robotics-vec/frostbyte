# ADR-008: Fahrmodell der Simulation

Status: angenommen (1. Nov 2026), Ticket SIM-02. Der Advisor (Opus) hat den Entwurf vor der Umsetzung geprüft.

## Entscheidung
- `sim/drive.hpp/.cpp` (nur Laptop): `sim::DriveModel` für einen Differentialantrieb. Eingang `step(dt_s, links_V, rechts_V)`, Ausgang wahre Pose (`x_in`, `y_in`, `h_deg`) und je Seite Weg und Tempo am Boden (`links_in`, `links_in_s`, …) – dort docken die Encoder-Modelle aus SIM-03 an.
- Jede Seite folgt der Kennlinie `V = kS·sgn(v) + kV·v + kA·a`, gerechnet direkt am Rad in in/s, in den Einheiten von `roboter.<name>.antrieb` in `sim/sim_params.json`. Haftreibung (bei |V| ≤ kS keine Bewegung) und Anhalten beim Vorzeichenwechsel wie in `hal/sim/sim_motor.hpp`. `SimMotor` wird nicht wiederverwendet: er rechnet in Grad am Motor, die Umrechnung bräuchte Raddurchmesser und Übersetzung, die nach `config/` gehören und noch TODO(HW) sind.
- Kinematik: `v = (vL + vR)/2`, `ω = (vR − vL)/spurbreite`. Integration halbimplizit mit dem Heading in der Mitte des Schritts (bleibt auf Kreisbögen, Test über 20 s).
- **Schlupf** nur als Grenze der Bodenbeschleunigung je Seite: `|a| ≤ mu·g` (`g` aus `welt.g_in_s2`). `masse_lb` geht nicht ein (steckt in kA; `mu·g` ist massenunabhängig). Durchdrehende Räder (Rad schneller als Boden) werden nicht modelliert: dafür fehlt die Radträgheit.
- **Akku**: neuer Eintrag `roboter.<name>.akku.spannung_V` (Schätzung 12,8 V). Er begrenzt den Befehl auf `±min(12 V, Akku)`. Kein Spannungseinbruch über die Zeit (keine Quelle dafür).
- **Kein Rauschen** im Fahrmodell: sim_params hat dafür keine Werte. `Rng` bleibt in SIM-02 unbenutzt.
- **Heading-Konvention** (bisher nur Annahme des Viewers, docs/notes.md SIM-07): Ursprung Feldmitte, 0° = +x = rote → blaue Station, gegen den Uhrzeigersinn positiv, `h_deg` in (−180, 180].
- Welt: je Roboter mit Startpose ein `DriveModel`, Reihenfolge je ms: Steuerung → Log des Zustands bei t → Physik t → t + schritt. Noch kein Regler, daher 0 V: die Roboter stehen an der Startpose, die CSV ist byte-gleich zu SIM-01 (bis auf `build=`). Ohne Startpose (null in field.json) kein Modell, Pose-Zellen bleiben leer.
- Tests nur mit Fixture-Werten (`test/fixtures/sim_params_min.json` bzw. Werte im Test), Posen mit `Approx` (sin/cos können zwischen glibc und Apple-libm im letzten Bit abweichen).

## Folgen / offen
- Beide Seiten sind unabhängig und benutzen dasselbe kA auch für die Drehung. Die Drehträgheit ist damit vereinfacht; SIM-08 kann ein eigenes kA für Drehung (`kA_dreh`) ergänzen, wenn die Systemidentifikation es zeigt.
- Kopplung an `hal::Motor` (Regler sehen Motoren, nicht Seiten) kommt mit dem Regler- bzw. Routen-Ticket; dann braucht es Raddurchmesser und Übersetzung aus `config/`.
- Schlupf greift mit den heutigen Schätzwerten nie (12 V aus dem Stand ≈ 228 bzw. 326 in/s² < mu·g ≈ 347 in/s²).
