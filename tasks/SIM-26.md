---
id: SIM-26
phase: "5"
titel: Odometriefehler beim frühen ersten goto (Runner: 72 statt 12 in) untersuchen
status: in_progress
owner: arjanssen
branch: ticket/sim-26
depends: [SIM-25]
hw: nein
---

## Ziel
Beim Lauf von `routes/skills_runner.route` ohne `toggle`-Zeilen (oder mit abgelehnten Toggles, SIM-25) startet das erste `goto` bei 0,3 s. Der Median von `odo_err_max_in` ist dann 72 in, mit den Toggles davor (erstes `goto` bei 9,8 s) nur 12 in. In der ersten Fahrt hinkt die Schätzung der wahren Pose um bis zu 70 in hinterher, die Timeouts laufen ab (Log: out/sim/run000_runner.csv, Spalten x_in/y_in gegen gx_in/gy_in/tx_in). Herausfinden, ob das ein Fehler im Odometrie-/IMU-Modell der Simulation ist (zeitabhängiger Effekt am Matchanfang?) oder ein Effekt der Route (Anfahren mit Vollgas, Schlupf), und beheben oder in sim_params begründen.

## Fertig, wenn …
- Ursache benannt (Test oder Messung) und in docs/notes.md oder einem ADR festgehalten
- Falls Fehler im Modell: Test zuerst, dann Fix; gleicher Seed = byte-gleiche Logs
- Laptop-Tests grün, `python tools/board.py validate` grün

## Kontext
ADR-033 (Ergänzung SIM-25, „Nebenbefund“), tasks/SIM-25.md, sim/world.cpp (Odometrie/IMU im Steuertakt).

## Hardware-Prüfung
—

## Übergabe
—

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-04 · arjanssen (KI) · Ticket angelegt (aus SIM-25, Advisor-Hinweis) · –
