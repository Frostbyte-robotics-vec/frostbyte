---
id: SIM-10
phase: "1b"
titel: Scorer nach RSC3 (Punktewertung der Simulation)
status: in_progress
owner: arjanssen
branch: ticket/sim-10
depends: [SIM-05]
hw: nein
---

## Ziel
Scorer, der aus dem Spielzustand die Punkte nach den Wertungsregeln (RSC3) berechnet. Aus SIM-05 ausgelagert.

## Fertig, wenn …
- Scorer stimmt mit 5 Handrechnungen überein
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün

## Kontext
Wertungsregeln: Game Manual v2.1 (liegt mit C-07, PR #39, in docs/ref/), RSC3 PDF-Seite 32. Auszug und Handrechnungen: docs/rules_scoring.md. ADR-024.

## Hardware-Prüfung
—

## Übergabe
Scorer sim/scorer.* mit Tests fertig, Laptop-Tests grün. Offen: Mensch bestätigt gegen die Webversion https://manuals.vex.com/v5/override/v2.1/ (von der KI schon geprüft, keine Abweichung) die Handrechnungen (docs/rules_scoring.md) und die Cup-Ausrichtung (opaque_lower).

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-01 · matelechner01 (KI) · aus SIM-05 ausgelagert (Review PR #15) · –
2026-11-03 · arjanssen (KI) · Wertungsregeln aus Manual v2.1, Scorer + 14 Testfälle + ADR-024; Handrechnungen von KI, Bestätigung offen · –
