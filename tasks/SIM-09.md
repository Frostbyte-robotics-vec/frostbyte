---
id: SIM-09
phase: "1b"
titel: Sensormodelle, Kollisionen und Regelprüfer in run_match einhängen
status: review
owner: matelechner01
branch: ticket/sim-09
depends: [SIM-03, SIM-05]
hw: nein
---

## Ziel
run_match benutzt das Fahrmodell (SIM-02), die Sensormodelle (SIM-03), die Mechanismen/Objekte (SIM-04) sowie Kollisionen und
Regelprüfer (SIM-05) und schreibt das Log mit den simulierten Messwerten. Bisher stehen die Modelle einzeln mit Tests da;
kein Ticket verdrahtet sie (Hinweis aus den Reviews zu PR #11 und #13).

## Fertig, wenn …
- Ein Match mit beiden Robotern läuft mit Fahrmodell, Sensoren (Profil none/realistic/harsh) und Kollisionen durch
- Regelverstöße (SG2, SG3, SG6) und Matchende-Halt werden im Lauf geprüft und im Bericht gemeldet
- Gleicher Seed = byte-gleiche Logs (Test)
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün

## Kontext
Regeln: CLAUDE.md. Vorher klären: ADR-Nummern der SIM-Tickets (008 doppelt belegt, siehe Review PR #11).

## Hardware-Prüfung
—

## Übergabe
Stand: run_match hängt Fahrmodell, Sensormodelle (Odometrie/IMU/GPS ins Log), Kollisionen (Roboter/Roboter, Feldrand), Regelprüfer (SG2 beim Start, Matchende-Halt) ein; Handsteuerung per RobotSetup::open_loop bis zum Routen-Ticket. Tests: test/test_sim_integration.cpp. Nicht lokal gebaut (Toolchain fehlt), CI entscheidet. Abhängigkeit SIM-05 war noch im Review: Branch baut auf ticket/sim-05 auf (PR #15 zuerst mergen). Offen: SG3 (Höhe nicht in sim_params), Kollision mit Feldelementen (field.json null), Score-Spalte (SIM-10), CLI-Option für das Rauschprofil.

Review PR #17 (KI, 2026-11-01), Empfehlung: ändern:
1. Blocker: zirkuläre Includes world.hpp <-> sensor_models.hpp, Rng unbekannt (CI rot) → Rng nach sim/rng.hpp.
2. Danach rot: test/test_sim_world.cpp:85 (erste Logzeile hat jetzt Sensorwerte).
3. Kriterium 2: SG3/SG6 werden im Lauf nicht geprüft (nur SG2 beim Start); Besitzmodell fehlt → prüfen oder als Ticket abspalten.
4. Frage: Bericht (sim_report.py = SIM-06?) und Ausgabe von rule_violations in sim/main.cpp.
5. Klein: CLI-Option Rauschprofil; Board-Status review setzen; nächste ADR-Nummer 013.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-01 · matelechner01 (KI) · Ticket angelegt (Lücke aus Reviews PR #11/#13) · –
2026-11-01 · matelechner01 (KI) · run_match-Integration samt Tests
