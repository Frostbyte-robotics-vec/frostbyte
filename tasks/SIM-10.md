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
blocked: Die Wertungsregeln (Punkte je Pin/Cup/Goal usw.) stehen nirgends im Repo und werden nicht geraten. Der Mensch
liefert den Regeltext bzw. die Wertungstabelle (z. B. docs/rules_scoring.md) und die 5 Handrechnungen.

## Hardware-Prüfung
—

## Übergabe
blocked: wartet auf Wertungsregeln und Handrechnungen vom Menschen.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-01 · matelechner01 (KI) · aus SIM-05 ausgelagert (Review PR #15) · –
