---
id: SIM-02
phase: "1b"
titel: Fahrmodell (kS, kV, kA, Spurbreite, Schlupf, Akkuspannung) aus sim/sim_params.json
status: done
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
Stand 2026-11-01: umgesetzt, Laptop-Tests grün (59 Fälle), beide Roboter bauen, validate ok. Entscheidungen in ADR-008.
- sim/drive.hpp/.cpp: DriveModel (Seiten-Kennlinie in in/s, Schlupf = |a| ≤ mu·g, Spannung ≤ min(motor_max_V, akku),
  Kinematik mit Mittelpunkt-Heading, Zustand in Grad, h_deg in (−180, 180] schon ab Start).
  sim_params.json: neu roboter.<name>.antrieb.motor_max_V (12 V) und roboter.<name>.akku.spannung_V (Schätzung 12,8 V).
- Welt: je Roboter mit Startpose ein DriveModel, noch 0 V (kein Regler). Welt-Test mit field_min.json: letzte Zeile
  tx/ty/th = Startpose. Mit dem echten field.json (alle Posen null) entsteht kein Modell, CLI-Ausgabe daher wie bei SIM-01.
- Tests (test/test_sim_drive.cpp, nur Fixture-Werte): Endtempo 12 V ± 3 % zur Formel (12 − kS)/kV, Anfahren mit tau,
  Haftreibung, Heading-Richtung, Punktdrehung (gemessen an der Pose), Kreisbogen ohne Drift, Quadrat, Schlupf (mu klein),
  Akku < 12 V, Laden aus JSON. Gegenprobe: 6 absichtlich eingebaute Fehler (Spurbreite, Euler, Schlupf, Akku,
  Vorzeichenwechsel, Haftreibung) machen je mindestens einen Test rot; nach dem Advisor-Review mit 8 Fehlern
  (zusätzlich Akku/Motor-Grenze einzeln, Wrap im Konstruktor) wiederholt.
Advisor-Rat (eingearbeitet): keine 12 V im Code (motor_max_V in sim_params), Heading-Zustand in Grad, Einheiten in allen
  lokalen Namen, Wrap im Konstruktor (sonst loggt Zeile 1 z. B. 270 statt −90), Byte-Vergleich ehrlich beschreiben.
Hinweis für die nächste Sitzung: board.py next bietet SIM-03 an (hängt nur an SIM-01), die Encoder-Modelle bauen aber
  auf links_in/rechts_in aus SIM-02 auf → erst nach dem Merge von SIM-02 beginnen oder SIM-04 vorziehen.
- „Quadrat 2 m“ als Seitenlänge 2 m = 78,74″ gelesen. Der Testregler arbeitet mit relativen Zielen aus den
  Seitenwegen, verglichen wird die wahre Pose; Heading an jeder Ecke geprüft (zwei 180°-Fehler schlössen sonst auch).
Offene Frage an einen Menschen: Soll docs/log_format.md die Heading-Konvention (gegen den Uhrzeigersinn positiv,
(−180, 180]) als klarstellenden Satz bekommen? Nicht still geändert (Format nur mit neuer Version).
Testen: cmake --build build-test && ./build-test/frostbyte_tests -tc="Fahrmodell*"
Am echten Roboter: nichts (Laptop); kS/kV/kA/mu/Akku sind Schätzungen, SIM-08 misst sie.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-01 · Arved Janssen (KI) · DriveModel, Tests, Welt-Einbau, ADR-008, Advisor-Review eingearbeitet · 209d672
