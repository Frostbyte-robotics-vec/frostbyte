---
id: SIM-02
phase: "1b"
titel: Fahrmodell (kS, kV, kA, Spurbreite, Schlupf, Akkuspannung) aus sim/sim_params.json
status: in_progress
owner: Arved Janssen
branch: ticket/sim-02
depends: [SIM-01]
hw: nein
---

## Ziel
Fahrmodell (kS, kV, kA, Spurbreite, Schlupf, Akkuspannung) aus sim/sim_params.json

## Fertig, wenn …
- 12 V geradeaus: Endtempo ± 3 % zur Formel; Quadrat 2 m schließt auf ≤ 0,5″ ohne Rauschen
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün


## Kontext
Details im Plan: docs/plan.md (Ticket SIM-02). Regeln: CLAUDE.md.

## Hardware-Prüfung
—

## Übergabe
(aktueller Stand für die nächste Sitzung – wird bei jedem Sitzungsende überschrieben)
Stand 2026-11-01: umgesetzt, Laptop-Tests grün (57 Fälle), beide Roboter bauen, validate ok. Entscheidungen in ADR-008.
- sim/drive.hpp/.cpp: DriveModel (Seiten-Kennlinie in in/s, Schlupf = |a| ≤ mu·g, Akku = Grenze min(12 V, akku),
  Kinematik mit Mittelpunkt-Heading). sim_params.json: neu roboter.<name>.akku.spannung_V (Schätzung 12,8 V).
- Welt: je Roboter mit Startpose ein DriveModel, noch 0 V (kein Regler) → CSV wie bei SIM-01 (bis auf build=), geprüft
  mit 20 Läufen gegen den Stand vor SIM-02.
- Tests (test/test_sim_drive.cpp, nur Fixture-Werte): Endtempo 12 V ± 3 % zur Formel (12 − kS)/kV, Anfahren mit tau,
  Haftreibung, Heading-Richtung, Punktdrehung (gemessen an der Pose), Kreisbogen ohne Drift, Quadrat, Schlupf (mu klein),
  Akku < 12 V, Laden aus JSON. Gegenprobe: 6 absichtlich eingebaute Fehler (Spurbreite, Euler, Schlupf, Akku,
  Vorzeichenwechsel, Haftreibung) machen je mindestens einen Test rot.
- „Quadrat 2 m“ als Seitenlänge 2 m = 78,74″ gelesen. Der Testregler arbeitet mit relativen Zielen aus den
  Seitenwegen, verglichen wird die wahre Pose; Heading an jeder Ecke geprüft (zwei 180°-Fehler schlössen sonst auch).
Offene Frage an einen Menschen: Soll docs/log_format.md die Heading-Konvention (gegen den Uhrzeigersinn positiv,
(−180, 180]) als klarstellenden Satz bekommen? Nicht still geändert (Format nur mit neuer Version).
Testen: cmake --build build-test && ./build-test/frostbyte_tests -tc="Fahrmodell*"
Am echten Roboter: nichts (Laptop); kS/kV/kA/mu/Akku sind Schätzungen, SIM-08 misst sie.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-01 · Arved Janssen (KI) · DriveModel, Tests, Welt-Einbau, ADR-008 · Branch ticket/sim-02
