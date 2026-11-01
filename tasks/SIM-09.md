---
id: SIM-09
phase: "1b"
titel: Modelle in run_match einhängen: Fahrmodell (SIM-02), Sensoren (SIM-03), Mechanismen und Ereignisse (SIM-04)
status: todo
owner:
branch:
depends: [SIM-02, SIM-03, SIM-04]
hw: nein
---

## Ziel
Modelle in run_match einhängen: Fahrmodell (SIM-02), Sensoren (SIM-03), Mechanismen und Ereignisse (SIM-04).
SIM-02 bis SIM-04 liefern die Modelle einzeln getestet, aber run_match nutzt sie noch nicht (siehe Übergaben von SIM-03 und SIM-04).

## Fertig, wenn …
- frostbyte_sim fährt beide Routen mit dem Fahrmodell; die Lokalisierung sieht nur die Sensor-Messwerte, nicht die wahre Pose
- Mechanismen und Objektereignisse laufen im Takt mit; das Log enthält die Felder nach docs/log_format.md (ohne Formatänderung, sonst neue Version)
- gleicher Seed = byte-gleiche Ausgabe; Profil none = Messwerte gleich wahre Werte (Test)
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün

## Kontext
Angelegt aus dem KI-Review von PR #11 (SIM-03), Review-Punkt 2. Details: ADR-008 (Fahrmodell), ADR-009 (Mechanismen), ADR-010 (Sensormodelle). Regeln: CLAUDE.md.

## Hardware-Prüfung
—

## Übergabe
(aktueller Stand für die nächste Sitzung – wird bei jedem Sitzungsende überschrieben)

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-01 · arjanssen (KI) · Ticket angelegt (Review PR #11) · –
