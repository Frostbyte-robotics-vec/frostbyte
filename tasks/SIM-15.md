---
id: SIM-15
phase: "1b"
titel: Live-Kollisionen im Browser (Feldrand, Feldelemente, Roboter–Roboter) mit COLLISION-Ereignis
status: in_progress
owner: arjanssen
branch: ticket/sim-15
depends: [SIM-14]
hw: nein
---

## Ziel
Im Live-Modus kann kein Roboter durch Rand, Feldelemente oder den anderen Roboter fahren. Jeder neue Kontakt erzeugt ein Ereignis COLLISION:<mit> (Token wie in docs/log_format.md, z. B. COLLISION:rand, COLLISION:runner, COLLISION:<feldelement-name>).

## Fertig, wenn …
- Geometrie als 1:1-Übertragung von sim/collision.cpp nach tools/viewer/livesim.js: penetration(Box, Box), penetration(Box, Kreis), push_inside_field – gleiche Konvention „Berührung ohne Überlappung ist keine Kollision“
- Umrisse aus sim_params.json, Feldelemente und Feldgröße aus field/field.json (null-Koordinaten werden übersprungen)
- Ereignis nur beim Beginn eines Kontakts, nicht in jedem Takt; hält der Kontakt > 0,5 s bei Fahreingabe, zusätzlich BLOCKED
- Node-Tests: dieselben Fälle wie die C++-Tests zu SIM-05 (Box/Box, Box/Kreis, Rand, Berührung = kein Kontakt), dazu: Fahrt gegen die Wand → Pose bleibt im Feld, genau ein COLLISION:rand
- Kollisionsumrisse über eine Taste (C) als Linien einblendbar
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün

## Kontext
Vom Menschen gewünscht (3.11.): Steuerung „mit Kollision“ für die Grundsimulation am 4.11. Abweichungen zum C++-Ergebnis prüft SIM-17. Regeln: CLAUDE.md.

## Hardware-Prüfung
—

## Übergabe
—

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-03 · DAVIDCOPCOP (KI) · Ticket angelegt · –
