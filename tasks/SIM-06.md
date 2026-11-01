---
id: SIM-06
phase: "1b"
titel: Monte-Carlo-Lauf, summary.json, tools/sim_report.py (HTML), Abnahmeregeln als Exit-Code, CI-Schritt
status: in_progress
owner: matelechner01
branch: ticket/sim-06
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
Stand: tools/sim_report.py (summary.json, HTML, Abnahme als Exit-Code) mit tools/test_sim_report.py (lokal grün); CI: Python-Tests, Job viewer-demo misst 100 Läufe ≤ 30 s und legt report.html ins Artefakt; Pages veröffentlicht viewer/report.html. Offen: P10/Median/P90 der PUNKTE und target_p10 erst mit dem Scorer (SIM-10, blockiert) – bis dahin meldet der Bericht „nicht geprüft“. „Beide Routen“ erst mit dem Routenspieler (A-03); bis dahin läuft das Demo.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-01 · matelechner01 (KI) · sim_report.py, Tests, CI-Schritte
