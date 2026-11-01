---
id: SIM-07
phase: "1b"
titel: Browser-Viewer tools/viewer/index.html: CSV laden, Feld, beide Roboter in 3D (Vorlage tools/viewer/reference/), Zeitregler, Overlay zweier Logs, Ereignisliste
status: done
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
— (kein Hardware-Ticket). Sobald der Roboter loggt: eine echte Roboter-CSV laden; bisher nur mit generierten Logs geprüft.
Dabei Heading-Konvention und Bedeutung von lift_deg am echten Roboter bestätigen (siehe offene Fragen).

## Übergabe
In Review: https://github.com/matelechner01/frostbyte/pull/3 (Code auf ticket/sim-07).

PR-Titel: SIM-07: Browser-Viewer für Logs (3D, Zeitregler, Overlay, Ereignisse)
PR-Text (Vorlage .github/pull_request_template.md):
  ## Ticket
  SIM-07 – Browser-Viewer tools/viewer/index.html  (tasks/SIM-07.md)
  ## Was wurde gemacht
  - tools/viewer/index.html: Feld 144″, beide Roboter in 3D (Modelle 1:1 aus reference/ via robots.js), Zeitregler mit
    Ereignis-Markierungen, Overlay beliebig vieler Logs über t_s (je Roboter erstes Log = Modell, weitere = Geist, Δ-Anzeige),
    Posen-Quelle Odometrie/GPS/wahr, Ereignisliste mit Filter und Sprung, Zustandstabelle, Versatz je Log, Draufsicht, Folgen.
  - Läuft per Doppelklick (file://); three.js r128 per CDN (ADR-005). Ohne Internet: Tabellen/Ereignisse gehen, kein 3D.
  - Starre Modellteile werden zusammengefasst: 3 Modelle 3491 → 345 Zeichenaufrufe. ?stats zeigt Bilder/s.
  - logparse.js (Spalten nach Namen, leer = NaN, Gruppierung nach robot-Spalte, Interpolation inkl. Winkel), make_test_logs.py.
  ## Fertig-Kriterien
  - [x] Sim-CSV und Roboter-CSV nebeneinander: make_test_logs.py erzeugt beide; per Chrome headless geprüft (Overlay, Δ 0,7″).
        „flüssig“: 345 Aufrufe/147k Dreiecke bei 3 Modellen; bitte am eigenen Rechner mit ?stats ansehen (headless rendert per Software).
  - [ ] Laptop-Tests grün, beide Roboter bauen: auf diesem Rechner nicht prüfbar (kein cmake/PROS, noch kein test/).
        Neue Tests: python3 -m unittest discover -s tools/viewer -p "test_*.py" → 16 ok (JS-Teil braucht macOS/osascript,
        läuft nicht in ctest/CI).
  - [x] python3 tools/board.py validate grün
  ## Am echten Roboter noch zu prüfen
  - echte Roboter-CSV laden; Heading-Konvention und lift_deg-Bedeutung bestätigen
  ## Review (Mensch)
  - [ ] Code gelesen und verstanden

Stand: tools/viewer/ index.html, logparse.js, logparse_test.js, robots.js, make_test_logs.py, test_*.py; ADR-005; docs/notes.md.
Testen: python3 tools/viewer/make_test_logs.py --out out/viewer_test; index.html doppelklicken, CSVs hineinziehen.
Oder python3 -m http.server 8765 im Repo und
http://localhost:8765/tools/viewer/index.html?selftest&stats&log=../../out/viewer_test/sim_stacker.csv&log=../../out/viewer_test/robot_stacker.csv
Offene Fragen (für Menschen):
- Heading-Konvention (Achse von 0°, Drehsinn) nicht in docs/log_format.md festgelegt; Annahme: +x = rot→blau, gegen den
  Uhrzeigersinn positiv (placeOnField in index.html).
- lift_deg: Liftwinkel am Gelenk oder Motorgrad? Viewer nimmt Liftwinkel an (Modellbereich −40…45° / −20…60°).
- Darf ein Log beide Roboter enthalten (Kopfzeile nennt einen, robot-Spalte existiert)? Viewer kann beides.
docs/plan.md hat keinen Abschnitt zu SIM-07, gearbeitet nach Ticket-Text.
Advisor-Rat umgesetzt: three.js fest auf r128 (ADR), file:// als Normalfall, Parser-Tests über osascript, Draw-Calls gemessen.

Review (KI, PR #3) „ändern“ – erledigt (2d6c422): (1) ADR → ADR-005; (2) Viewer-Tests: `python3 -m unittest discover -s tools/viewer -p "test_*.py"` → 16 ok, 0 übersprungen auf macOS, Gegenprobe (Mutation in logparse.js) schlägt fehl; (3) Heading/lift_deg im PR unter „Am echten Roboter“.
main in den Branch eingemergt (kein Rebase); dabei docs/notes.md wiederhergestellt (Kopf + S-01-Einträge, bei PR #4 verloren).
Offen: Roboter-Builds (keine ARM-Toolchain auf diesem Rechner), Bildrate im Browser (Mensch, ?stats), Node-Runner für Windows/CI (Vorschlag für S-03).

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-01 · Arved Janssen (KI) · Viewer, Parser, Testlog-Generator, ADR-005, Performance · 395c17d
2026-11-01 · Arved Janssen (KI) · PR #3 geöffnet, Status review · –
2026-11-01 · Arved Janssen (KI) · Review-Punkte PR #3 erledigt, main eingemergt · 2d6c422
