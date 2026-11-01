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
1. Test für gps.sigma_deg (1000 Proben, ±10 %) ergänzen.
2. Einhängen der Sensormodelle in run_match einem Ticket zuweisen.
3. Vor Merge auf main rebasen.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-01 · DAVIDCOPCOP · Sensormodelle samt Tests, Notes-Eintrag
