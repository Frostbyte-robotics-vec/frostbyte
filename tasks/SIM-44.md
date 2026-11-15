---
id: SIM-44
phase: "5"
titel: GPS-Fusion in der Simulation verdrahten (Odometrie springt bei Kontakt und wird nie zurückgezogen)
status: review
owner: arjanssen
branch: ticket/sim-44
depends: [SIM-26]
hw: nein
---

## Ziel
Befund aus 20 Läufen (Skills-Routen, Seed 1–20, 2026-11-15): Odometriefehler am Ende im Median Runner 7,6 in, Stacker 2,4 in. Der Fehler springt bei Kontakten (Runner, Lauf 0: 0,14 → 2,8 in bei t = 3 s, 3,1 → 5,8 in bei 4,5 s, 3,3 → 7,5 in bei 25 s) und bleibt danach konstant, bis Matchende. Ursache: `lib/estimation/gps_fusion.hpp` (D-04) wird in der Simulation nicht benutzt. `sim/world.cpp` führt `r.est` nur per Odometrie (Zeile ~376) und das GPS-Modell wird nur ins Log geschrieben. Die Routen laufen also auf einer Pose, die der echte Roboter mit GPS zurückziehen würde. (Vermutung beim Anlegen: Punkte pessimistisch. Gemessen das Gegenteil: siehe ADR-016-Nachtrag, die alte Punktzahl war zu hoch.)

Zu klären/zu tun:
- GPS-Messung im Steuertakt (nicht nur beim Logschritt) an `GpsFusion::update` geben; `r.est` ist die fusionierte Pose, die Route und Log sehen.
- Parameter (`max_error_in`, `max_speed_in_s`, `gate_in`, `position_gain`, `heading_gain`, `resync_samples`) in `sim/sim_params.json` mit `quelle: "schaetzung"` (config/*.hpp hat sie als TODO(HW), nicht raten); Schalter zum Ausschalten, um vorher/nachher zu vergleichen.
- Log-Format bleibt (x_in = Pose, die der Roboter benutzt); Kopfzeile/`log_format.md` nur anfassen, wenn nötig (dann neue Versionsnummer).
- ADR-Nachtrag (Sim-Pipeline: Odometrie → Fusion → Route).

## Fertig, wenn …
- Roter Test zuerst: Wegmesser-Drift (Test, 10 % Skalierungsfehler, rund 4 in) wird mit Fixture-Parametern (nicht Platzhaltern aus sim_params.json) auf ≤ 1 in zurückgezogen. Kontaktsprünge (mehrere Zoll auf einmal) sind nur in der 20-Läufe-Messung im ADR belegt (Endfehler 7,6 → 0,09 in), nicht als eigener Test: die Simulation erzeugt keinen reproduzierbaren Sprung ohne Route; ohne GPS (Aussetzer) bleibt es bei Odometrie
- Gleicher Seed = byte-gleiches Ergebnis (Test), 20 Läufe vorher/nachher als Zahlen im ADR-Nachtrag (Odometriefehler am Ende, Punkte, Kontakte)
- Laptop-Tests grün, beide Roboter bauen, `board.py validate` grün, clang-format ok

## Kontext
lib/estimation/gps_fusion.hpp, sim/world.cpp, sim/sensor_models.*, sim/sim_params.json, ADR-016, ADR-010, SIM-26, D-04.
Die Fusionswerte sind Platzhalter; am echten Roboter müssen sie aus GPS-Logs abgeleitet werden (TODO(HW) in config/).

## Hardware-Prüfung
—

## Übergabe
(aktueller Stand für die nächste Sitzung – wird bei jedem Sitzungsende überschrieben)
Stand 2026-11-15 (fertig, PR offen): Fusion in sim/world.cpp verdrahtet (SensorParams.fusion, Block sensoren.fusion in sim_params.json, `aktiv`), test/test_sim_fusion.cpp (4 Tests), Laptop-Tests, Python-Tests, beide Roboter-Builds, validate grün. Odometriefehler am Ende (Median, 20 Läufe) Runner 7,63 → 0,09 in, Stacker 2,44 → 0,09 in. Punkte Median 46 → 16: Ursache gefunden (ADR-016-Nachtrag): Runner-Toggle TOGGLE_RED_1 wird mit ehrlicher Pose nicht mehr erreicht (wahre Distanz 12,6 > 12 in; vorher 10,5 durch Zufallsdrift), rote Neutral-Goals zählen ohne roten Toggle nicht (scorer.cpp:18). Die alte Punktzahl war zu hoch. Folge-Ticket SIM-45 (Routen: Anfahrposen am Toggle). Am echten Roboter: Fusionswerte aus GPS-Logs (TODO(HW) in config/).

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-15 · arjanssen (KI) · Ticket angelegt (Befund aus 20 Läufen) · –
2026-11-15 · arjanssen (KI) · Fusion verdrahtet, Tests, ADR-016-Nachtrag, Punkteeinbruch erklärt · –
