---
id: SIM-19
phase: "1b"
titel: Log leeren (restartLog) zeichnet nur auf dem log_ms-Raster auf
status: todo
owner:
branch: ticket/sim-19
depends: [SIM-16, SIM-18]
hw: nein
---

## Ziel
Aus dem KI-Review von PR #45 (SIM-16), vor dem Merge nicht mehr eingearbeitet: `restartLog()` (livesim.js:252) ruft `record()` zum aktuellen Schritt auf. „Log leeren“ mitten im Raster (z. B. nach 105 Schritten) erzeugt als erste Zeile t_s = 0.105 statt auf dem log_ms-Raster.

## Fertig, wenn …
- restartLog zeichnet nur bei `stepNo % logEvery === 0` auf (R/Start sind bei 0 immer drauf)
- Node-Test: Log bei einem Nicht-Vielfachen leeren → alle Zeilen außer der Schlusszeile liegen auf dem Raster
- Die ADR-Aussage „einzige Zeile abseits des Rasters ist die Schlusszeile“ stimmt
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün

## Kontext
Gleiche Dateien wie SIM-18 (tools/viewer/), erst nach SIM-18 beginnen, um Konflikte zu vermeiden. Regeln: CLAUDE.md.

## Hardware-Prüfung
—

## Übergabe
—

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-03 · arjanssen (KI) · Ticket angelegt aus offenem Review-Punkt von PR #45 · –
