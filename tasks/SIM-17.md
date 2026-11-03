---
id: SIM-17
phase: "1b"
titel: Abgleich Browser-Livesim gegen frostbyte_sim (gleicher Eingabeverlauf → gleiche Posen)
status: in_progress
owner: arjanssen
branch: ticket/sim-17
depends: [SIM-15]
hw: nein
---

## Ziel
Sicherstellen, dass livesim.js nicht still vom C++-Modell abweicht. Nicht für den 4.11. nötig.

## Fertig, wenn …
- frostbyte_sim kann einen festen Spannungsverlauf (Datei) statt einer Route fahren und schreibt die Posen ohne Rauschen (--noise none)
- Ein Fixture-Verlauf (geradeaus, Drehung, Fahrt gegen die Wand) liefert in C++ und JS Posen mit Abweichung ≤ 0,1″ / 0,5° über 10 s; Test läuft in der CI
- Ergebnis und gewählte Toleranz im ADR aus SIM-14 nachgetragen
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün

## Kontext
Folgt aus der Entscheidung in SIM-14 (Modell im Browser doppelt). Regeln: CLAUDE.md.

## Hardware-Prüfung
—

## Übergabe
Fertig, bereit für /finish. Eingabeverlauf-CSV + `frostbyte_sim --input`, Browser-Hook `voltsAt`, Referenz in test/fixtures, zwei Tests (C++ ctest, JS test_livesim.py). Größte Abweichung 0,0007 in / 0,0005° (Log-Rundung). Details im ADR-025-Nachtrag SIM-17.
Lokal: node fehlt, der JS-Test wurde über osascript (JavaScriptCore) ausgeführt und ist grün; Node läuft erst in der CI. Roboter-Build lokal nicht prüfbar (keine Toolchain); es wurde kein Roboter-Code geändert.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-03 · DAVIDCOPCOP (KI) · Ticket angelegt · –
2026-11-03 · arjanssen (KI) · Eingabeverlauf, --input, voltsAt-Hook, Referenz und Abgleichstests, ADR-Nachtrag · –
