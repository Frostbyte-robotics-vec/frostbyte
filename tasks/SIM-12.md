---
id: SIM-12
phase: "1b"
titel: Viewer und Demo-Logs auf GitHub Pages veröffentlichen
status: review
owner: matelechner01
branch: ticket/sim-12
depends: [SIM-11]
hw: nein
---

## Ziel
Der Pages-Workflow baut frostbyte_sim, erzeugt die Demo-Logs und veröffentlicht tools/viewer samt Demo unter <Pages-URL>/viewer/. Der Viewer lädt die Demo ohne Parameter von selbst.

## Fertig, wenn …
- Workflow baut auf main, Seite zeigt beide Roboter in 3D mit Zeitregler (Demo wird automatisch geladen)
- Viewer funktioniert weiter lokal per Datei-Auswahl
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün

## Kontext
Vom Menschen priorisiert (1.11.): die Simulation soll am nächsten Morgen im Browser ansehbar sein. Regeln: CLAUDE.md.

## Hardware-Prüfung
—

## Übergabe
(aktueller Stand für die nächste Sitzung – wird bei jedem Sitzungsende überschrieben)

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-01 · matelechner01 (KI) · Ticket angelegt · –
