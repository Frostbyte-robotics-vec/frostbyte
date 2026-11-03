---
id: SIM-13
phase: "1b"
titel: Eigene Simulationsseite <Pages-URL>/sim/ – nur 3D-Ansicht und Ereignis-Log
status: in_progress
owner: DAVIDCOPCOP
branch: ticket/sim-13
depends: [SIM-12]
hw: nein
---

## Ziel
Eine schlanke, interne Seite unter <Pages-URL>/sim/, die nur zwei Dinge zeigt: das 3D-Feld mit den Robotern (samt Zeitregler) und daneben einen Tab „Log“ mit den Ereignissen (COLLISION, BLOCKED, TIMEOUT, RULE, PLACED …). Kein Kopftext, keine Zustandstabelle, keine Log-Liste, keine Erklärtexte. Muss nicht schön sein.
Der bestehende Viewer unter /viewer/ bleibt unverändert erreichbar.

## Fertig, wenn …
- <Pages-URL>/sim/ lädt die Demo automatisch (wie SIM-12) und zeigt nur: 3D-Ansicht, Zeitleiste mit ▶/Geschwindigkeit, Log-Tab mit Ereignissen und Filter-Chips
- Ein Klick auf ein Ereignis im Log springt zur Zeit des Ereignisses
- Kein doppelter Code: dieselben Dateien wie der Viewer (index.html, logparse.js, robots.js), Umschaltung über einen schlanken Modus (z. B. `?ansicht=sim` bzw. `<body data-ansicht="sim">` in einer kleinen sim/index.html); dashboard.yml legt die Seite nach site/sim/
- /viewer/ sieht aus und funktioniert wie vorher; die Board-Seite verlinkt zusätzlich „Simulation (intern)“
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün

## Kontext
Vom Menschen gewünscht (3.11.): eine eigene Seite nur für die Simulation, intern, ohne Beiwerk. Grundlage für SIM-14 bis SIM-16 (Live-Handsteuerung). Pages-Workflow: .github/workflows/dashboard.yml. Regeln: CLAUDE.md.

## Hardware-Prüfung
—

## Übergabe
—

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-03 · DAVIDCOPCOP (KI) · Ticket angelegt · –
