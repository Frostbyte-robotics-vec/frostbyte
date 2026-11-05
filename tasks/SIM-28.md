---
id: SIM-28
phase: "5"
titel: Stau-Modell für Intake-Rollen in der Simulation (Folge von M-04)
status: review
owner: arjanssen
branch: ticket/sim-28
depends: []
hw: nein
---

## Ziel
M-04 (ADR-037) hat Stau-Erkennung und Rückwärtsimpuls als reine Logik, getestet gegen eine kleine Teststrecke. Die Simulation hat kein Stau-Modell. Hier entsteht eines: Rollen klemmen mit einer Wahrscheinlichkeit (Parameter in sim/sim_params.json, `quelle: schaetzung` bis Messung) und lösen sich nach einem Rückwärtsimpuls oder nie; der IntakeSubsystem läuft gegen diese Strecke.

## Fertig, wenn …
- Parameter nur in sim/sim_params.json; gleicher Seed = byte-gleiches Ergebnis
- Test: bei klemmenden Rollen löst der Impuls den Stau, bei hartnäckigem Stau endet der Lauf in `error (jam)`; Tests hängen nicht von Platzhalterwerten ab
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün

## Kontext
docs/decisions/ADR-037-intake.md, lib/subsystems/intake.hpp. Baut auf M-04 auf (PR #69 muss gemergt sein; liegt die Datei nicht auf main → überspringen); keine Board-Abhängigkeit, da M-04 erst nach dem Roboter-Test done wird.

## Übergabe
Fertig: sim/intake_model.{hpp,cpp}, Parameter ereignisse.intake_stau in sim/sim_params.json, test/test_sim_intake_jam.cpp, ADR-037-Nachtrag. Nicht im Match verdrahtet: Folgeticket SIM-31. Am Roboter nichts zu prüfen (reine Simulation); echte Klemm-/Lösewerte kommen aus M-05.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-05 · arjanssen (KI) · Stau-Modell IntakeRollers, Parameter, Tests, ADR-Nachtrag · ticket/sim-28
