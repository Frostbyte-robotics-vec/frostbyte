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
— (keine Hardware; im Browser einmal Live starten, gegen Wand fahren, C drücken)

## Übergabe
Stand: fertig, PR offen. livesim.js hat Kollisionsgeometrie (1:1 aus collision.cpp/world.cpp), Ereignisse (`takeEvents()`), Umriss (`outline()`); index.html: Taste C zeigt Umrisse, letzte Ereignisse im Info-Feld. 62 JS-Prüfungen grün (lokal via osascript, node fehlt auf diesem Mac; CI nutzt node); Gegenprobe ohne `collide()` lässt 7 Prüfungen scheitern.
Abweichungen vom Ticket (siehe ADR-025 Nachtrag): Token `COLLISION:wall` statt `rand` (wie C++/Log); Feldelemente: `live.kollision.radius_in.goals = 4.0` in sim_params.json als schaetzung (nicht gemessen; C++ kollidiert nicht mit Goals) – SIM-17 muss das abgleichen. Runner-Ersatzpose nach (-62, 40) verschoben (lag im Goal NEUTRAL_R1). Advisor: Radius eintragen statt weglassen, Abweichung wall/rand im PR prominent nennen.
Nicht im Browser getestet (nur Logik unter JavaScriptCore): Anzeige der Linien in index.html bitte kurz ansehen. Roboter-Build: `make` meldet hier nur „Nothing to be done for quick“, Roboter-Code unverändert.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-03 · DAVIDCOPCOP (KI) · Ticket angelegt · –
2026-11-03 · arjanssen (KI) · Kollisionen, Tests, ADR-Nachtrag, Anzeige · s. Branch
