---
id: SIM-11
phase: "1b"
titel: frostbyte_sim --demo: Demo-Szenario (Handsteuerung, Platzhalter-Posen) und --noise-Option
status: in_progress
owner: matelechner01
branch: ticket/sim-11
depends: [SIM-09]
hw: nein
---

## Ziel
frostbyte_sim --demo erzeugt ohne Routen und ohne vermessenes Feld zwei Logs, in denen beide Roboter fahren, sich berühren und der Feldrand wirkt. Die Logs sind als DEMO gekennzeichnet (build=demo), die Posen sind Platzhalter, keine Feldwahrheit. --noise none|realistic|harsh wählt das Rauschprofil.

## Fertig, wenn …
- Test: Demo ist deterministisch, enthält COLLISION-Ereignisse, keine Regelverstöße, Motoren stehen bei 0:00
- --demo und --noise im Aufruf dokumentiert (Kopf von sim/main.cpp)
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün

## Kontext
Vom Menschen priorisiert (1.11.): die Simulation soll am nächsten Morgen im Browser ansehbar sein. Regeln: CLAUDE.md.

## Hardware-Prüfung
—

## Übergabe
Stand: sim/demo.* (apply_demo), frostbyte_sim --demo/--noise, Tests in test/test_sim_demo.cpp. Baut auf ticket/sim-09 (PR #17) auf. Nicht lokal gebaut, CI entscheidet. Die Fahrfolgen sind Platzhalter; ob sich die Roboter wirklich berühren, prüft der Test.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-01 · matelechner01 (KI) · Ticket angelegt · –
2026-11-01 · matelechner01 (KI) · Demo-Szenario, --demo und --noise
