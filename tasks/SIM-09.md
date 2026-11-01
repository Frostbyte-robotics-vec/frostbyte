---
id: SIM-09
phase: "1b"
titel: Sensormodelle, Kollisionen und Regelprüfer in run_match einhängen
status: in_progress
owner: matelechner01
branch: ticket/sim-09
depends: [SIM-03, SIM-05]
hw: nein
---

## Ziel
run_match benutzt das Fahrmodell (SIM-02), die Sensormodelle (SIM-03), die Mechanismen/Objekte (SIM-04) sowie Kollisionen und
Regelprüfer (SIM-05) und schreibt das Log mit den simulierten Messwerten. Bisher stehen die Modelle einzeln mit Tests da;
kein Ticket verdrahtet sie (Hinweis aus den Reviews zu PR #11 und #13).

## Fertig, wenn …
- Ein Match mit beiden Robotern läuft mit Fahrmodell, Sensoren (Profil none/realistic/harsh) und Kollisionen durch
- Regelverstöße (SG2, SG3, SG6) und Matchende-Halt werden im Lauf geprüft und im Bericht gemeldet
- Gleicher Seed = byte-gleiche Logs (Test)
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün

## Kontext
Regeln: CLAUDE.md. Vorher klären: ADR-Nummern der SIM-Tickets (008 doppelt belegt, siehe Review PR #11).

## Hardware-Prüfung
—

## Übergabe
(aktueller Stand für die nächste Sitzung – wird bei jedem Sitzungsende überschrieben)

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-01 · matelechner01 (KI) · Ticket angelegt (Lücke aus Reviews PR #11/#13) · –
