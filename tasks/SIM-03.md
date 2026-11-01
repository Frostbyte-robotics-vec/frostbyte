---
id: SIM-03
phase: "1b"
titel: Sensormodelle: Tracking-Rad, Encoder, IMU-Drift, GPS mit Güte und Aussetzern
status: review
owner: DAVIDCOPCOP
branch: ticket/sim-03
depends: [SIM-01]
hw: nein
---

## Ziel
Sensormodelle: Tracking-Rad, Encoder, IMU-Drift, GPS mit Güte und Aussetzern

## Fertig, wenn …
- gemessene Statistik in 1000 Proben ± 10 % der Vorgabe
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün


## Kontext
Details im Plan: docs/plan.md (Ticket SIM-03). Regeln: CLAUDE.md.

## Hardware-Prüfung
—

## Übergabe
Stand: Modelle fertig und getestet (sim/sensor_models.hpp/.cpp, test/test_sim_sensor_models.cpp). Odometrie (Tracking/Encoder), IMU (Drift+Rauschen), GPS (Rauschen, Aussetzer, Güte~Geschwindigkeit), Rauschprofil skaliert alles. Noch nicht in run_match eingehängt: das braucht das Fahrmodell (SIM-02) für wahre Wege/Posen. Neuer Schlüssel gps.sigma_deg (Schätzung). Entscheidungen in ADR-010. Aussetzer-Test mit eigenem Wert 0,5 (1000 Proben).

## Review-Punkte (KI, PR #11)
1. erledigt: Test „GPS: Heading-Rauschen +-10 %“ (1000 Proben).
2. erledigt: neues Ticket SIM-09 (Modelle in run_match einhängen).
3. erledigt: main eingemergt (SIM-02, SIM-04); ADR-008-sensormodelle → ADR-010 (Nummer war doppelt).

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-01 · DAVIDCOPCOP · Sensormodelle samt Tests, Notes-Eintrag
2026-11-01 · arjanssen (KI) · main eingemergt, ADR → 010, sigma_deg-Test, Review-Punkte erledigt
4. ADR-Nummer: ADR-008-sensormodelle kollidiert mit ADR-008-fahrmodell auf main; umbenennen (nächste freie: ADR-012).
