---
id: SIM-06
phase: "1b"
titel: Monte-Carlo-Lauf, summary.json, tools/sim_report.py (HTML), Abnahmeregeln als Exit-Code, CI-Schritt
status: todo
owner:
branch:
depends: [SIM-02, SIM-03, SIM-05, SIM-09]
hw: nein
---

## Ziel
Monte-Carlo-Lauf, summary.json, tools/sim_report.py (HTML), Abnahmeregeln als Exit-Code, CI-Schritt

## Fertig, wenn …
- 100 Läufe beider Routen in ≤ 30 s; Bericht zeigt P10/Median/P90 und Fehler pro Zustand
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün


## Kontext
Details im Plan: docs/plan.md (Ticket SIM-06). Regeln: CLAUDE.md.

## Hardware-Prüfung
—

## Übergabe
(aktueller Stand für die nächste Sitzung – wird bei jedem Sitzungsende überschrieben)

Review PR #21 (KI, arjanssen): „Fehler pro Zustand“ fehlt im Bericht (tools/sim_report.py); Fertig-Kriterium ergänzen oder mit Mensch anpassen.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
