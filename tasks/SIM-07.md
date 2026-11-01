---
id: SIM-07
phase: "1b"
titel: Browser-Viewer tools/viewer/index.html: CSV laden, Feld, beide Roboter in 3D (Vorlage tools/viewer/reference/), Zeitregler, Overlay zweier Logs, Ereignisliste
status: in_progress
owner: Arved Janssen
branch: ticket/sim-07
depends: []
hw: nein
---

## Ziel
Browser-Viewer tools/viewer/index.html: CSV laden, Feld, beide Roboter in 3D (Vorlage tools/viewer/reference/), Zeitregler, Overlay zweier Logs, Ereignisliste

## Fertig, wenn …
- Sim-CSV und eine Roboter-CSV laufen flüssig nebeneinander (Test-CSV selbst erzeugen)
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün


## Kontext
Details im Plan: docs/plan.md (Ticket SIM-07). Regeln: CLAUDE.md.

## Hardware-Prüfung
—

## Übergabe
Stand: Viewer fertig, wartet auf Review.
- tools/viewer/index.html (UI, three.js r128 per CDN, ADR-002), logparse.js (Parser), robots.js (Modelle 1:1 aus reference/),
  make_test_logs.py (Testlogs), Tests: python3 -m unittest discover -s tools/viewer -p "test_*.py" (16 Tests, JS über osascript).
- Testen: python3 tools/viewer/make_test_logs.py --out out/viewer_test; index.html doppelklicken, die drei CSVs hineinziehen.
  Oder: python3 -m http.server 8765 im Repo, dann
  http://localhost:8765/tools/viewer/index.html?selftest&log=../../out/viewer_test/sim_stacker.csv&log=../../out/viewer_test/robot_stacker.csv
- Geprüft per Chrome headless (Screenshot, file:// und http): 3D, Overlay Sim↔Roboter (Δ-Anzeige), Ereignisliste, Selbsttest 39/39.
Offene Fragen:
- Heading-Konvention (Achse von 0°, Drehsinn) nicht in docs/log_format.md festgelegt; Annahme in placeOnField (index.html).
- lift_deg: Liftwinkel am Gelenk oder Motorgrad? Viewer nimmt Liftwinkel an (Bereich der Modelle −40…45° / −20…60°).
- Darf ein Log beide Roboter enthalten (Kopfzeile nennt einen, robot-Spalte existiert)? Viewer kann beides.
Nicht prüfbar auf diesem Rechner: „Laptop-Tests grün, beide Roboter bauen“ (kein cmake, kein PROS, kein test/-Ordner);
„läuft flüssig“ nur mit echtem Browser (headless rendert per Software) → beim Review am eigenen Rechner ansehen.
docs/plan.md hat keinen Abschnitt zu SIM-07, gearbeitet nach Ticket-Text.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-01 · Arved Janssen (KI) · Viewer, Parser, Testlog-Generator, ADR-002 · ticket/sim-07
