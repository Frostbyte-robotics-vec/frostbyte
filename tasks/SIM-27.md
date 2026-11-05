---
id: SIM-27
phase: "5"
titel: Kreuzende Routen von Stacker und Runner (Kollision bei ~2 s um x≈−55, y≈−10) auflösen
status: in_progress
owner: arjanssen
branch: ticket/sim-27
depends: [SIM-26]
hw: nein
---

## Ziel
Seit SIM-26 stimmt die Odometrie bei Kollisionen. Dabei zeigt sich: Beim Lauf von `routes/skills_stacker.route` und `routes/skills_runner.route` fährt der Runner bei etwa 2 s in den Stacker und bleibt hängen (Log: out/sim/run000_runner.csv, Ereignis COLLISION:stacker). Ursache in den Routen (Wege kreuzen sich), nicht im Modell. Herausfinden, wo, und Routen bzw. Startreihenfolge so ändern, dass kein Roboterkontakt in den ersten Sekunden entsteht. Prüfen, ob A-05 (Routen optimieren) das ohnehin abdeckt, sonst hier lösen.

## Fertig, wenn …
- Die Stelle ist benannt (Zeit, Posen aus dem Log) und im Ticket festgehalten
- Median der Roboterkontakte (sim_report) in den 20 Läufen ist 0 oder begründet kleiner als vorher
- Laptop-Tests grün, `python tools/board.py validate` grün

## Kontext
docs/notes.md (SIM-26), tasks/SIM-26.md, tasks/A-05.md (blockiert), sim/world.cpp (Kollision).

## Hardware-Prüfung
—

## Übergabe
Stelle: Stacker (Start −59/0) fuhr zum südlichen Loader LOADER_RED_1, Runner (Start −40/−64,5) zum nördlichen LOADER_RED_2; die Wege kreuzten sich bei ~2 s um x≈−55, y≈−10. Fix: Loader getauscht (Stacker LOADER_RED_2, Runner LOADER_RED_1). 20 Läufe Seed 1–20 mit den Routen: Kontakte Roboter Median 9 → 0, Feldrand 8–10 → 7–8. Punkte unverändert 16 (Ziel 120 ist A-05, nicht dieses Ticket). Wirkt nur in der Simulation mit geschätzten Maßen (Footprint/Kollisionsumriss aus sim_params); echte Wege am Roboter nicht geprüft. Kein eigener Test, weil das Ergebnis an Platzhalterwerten in sim_params hängt (CLAUDE.md). Nächster Schritt: PR.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-04 · arjanssen (KI) · Ticket angelegt (aus SIM-26) · –
2026-11-05 · arjanssen (KI) · Loader getauscht, Kontakte 9 → 0 · –
