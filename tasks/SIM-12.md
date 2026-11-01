---
id: SIM-12
phase: "1b"
titel: Viewer und Demo-Logs auf GitHub Pages veröffentlichen
status: in_progress
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
Stand: dashboard.yml baut frostbyte_sim, erzeugt Demo-Logs und legt Viewer + Demo + field.json nach site/viewer/ (Board-Seite verlinkt „Simulation ansehen“); Viewer lädt demo/run000_*.csv ohne Parameter (?nodemo schaltet ab, Feld-Fallback field.json). ci.yml: Job viewer-demo prüft die Pipeline schon im PR und stellt das Artefakt viewer-demo bereit. Der Pages-Workflow selbst läuft nur auf main und ist deshalb erst nach dem Merge belegt. Nicht lokal gebaut.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-01 · matelechner01 (KI) · Ticket angelegt · –
2026-11-01 · matelechner01 (KI) · Pages-Veröffentlichung, Viewer-Autoload, CI-Artefakt
