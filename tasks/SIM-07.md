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
Stand: ADR-002 (three.js r128 per CDN) und Testlog-Generator tools/viewer/make_test_logs.py mit Tests
(python3 -m unittest discover -s tools/viewer -p "test_*.py", prüft Spalten gegen docs/log_format.md) fertig.
Nächster Schritt: tools/viewer/index.html.
Hinweis: docs/plan.md hat keinen Abschnitt zu SIM-07, gearbeitet nach Ticket-Text.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
