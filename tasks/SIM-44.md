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
Befund aus 20 Läufen (Skills-Routen, Seed 1–20, 2026-11-15): Odometriefehler am Ende im Median Runner 7,6 in, Stacker 2,4 in. Der Fehler springt bei Kontakten (Runner, Lauf 0: 0,14 → 2,8 in bei t = 3 s, 3,1 → 5,8 in bei 4,5 s, 3,3 → 7,5 in bei 25 s) und bleibt danach konstant, bis Matchende. Ursache: `lib/estimation/gps_fusion.hpp` (D-04) wird in der Simulation nicht benutzt. `sim/world.cpp` führt `r.est` nur per Odometrie (Zeile ~376) und das GPS-Modell wird nur ins Log geschrieben. Die Routen laufen also auf einer Pose, die der echte Roboter mit GPS zurückziehen würde; Zielgenauigkeit und Punkte der Simulation sind dadurch pessimistisch bzw. unrealistisch.

Zu klären/zu tun:
- GPS-Messung im Steuertakt (nicht nur beim Logschritt) an `GpsFusion::update` geben; `r.est` ist die fusionierte Pose, die Route und Log sehen.
- Parameter (`max_error_in`, `max_speed_in_s`, `gate_in`, `position_gain`, `heading_gain`, `resync_samples`) in `sim/sim_params.json` mit `quelle: "schaetzung"` (config/*.hpp hat sie als TODO(HW), nicht raten); Schalter zum Ausschalten, um vorher/nachher zu vergleichen.
- Log-Format bleibt (x_in = Pose, die der Roboter benutzt); Kopfzeile/`log_format.md` nur anfassen, wenn nötig (dann neue Versionsnummer).
- `sim_report`: eine Kennzahl, ob die Fusion aktiv war.
- ADR-Nachtrag (Sim-Pipeline: Odometrie → Fusion → Route).

## Fertig, wenn …
- Roter Test zuerst: Kontaktsprung von mehreren Zoll in der Simulation wird mit Fixture-Parametern (nicht Platzhaltern aus sim_params.json) auf ≤ 1 in zurückgezogen; ohne GPS (Aussetzer) bleibt es bei Odometrie
- Gleicher Seed = byte-gleiches Ergebnis (Test), 20 Läufe vorher/nachher als Zahlen im ADR-Nachtrag (Odometriefehler am Ende, Punkte, Kontakte)
- Laptop-Tests grün, beide Roboter bauen, `board.py validate` grün, clang-format ok

## Kontext
lib/estimation/gps_fusion.hpp, sim/world.cpp, sim/sensor_models.*, sim/sim_params.json, ADR-016, ADR-010, SIM-26, D-04.
Die Fusionswerte sind Platzhalter; am echten Roboter müssen sie aus GPS-Logs abgeleitet werden (TODO(HW) in config/).

## Hardware-Prüfung
—

## Übergabe
(aktueller Stand für die nächste Sitzung – wird bei jedem Sitzungsende überschrieben)

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-15 · arjanssen (KI) · Ticket angelegt (Befund aus 20 Läufen) · –
