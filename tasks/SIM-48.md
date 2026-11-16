---
id: SIM-48
phase: "5"
titel: goto bleibt 0,5–1 in seitlich vom Ziel stehen (Differentialantrieb korrigiert quer nicht) → Timeout statt Ankunft
status: review
owner: arjanssen
branch: ticket/sim-48
depends: [SIM-44]
hw: nein
---

## Ziel
Abgespalten aus SIM-46 (KI-Review PR #127, Punkt 2). Befund (20 Läufe, Fusion an, ehrliche Pose): Der Stacker erreicht bei `goto NEUTRAL_R1` die Toleranz nicht (Lauf 0: Schätzpose 0,9 in vom Ziel, Roboter steht, Kurs −37° etwa 90° quer zum Fehlervektor), `move_to` läuft in den Timeout (5 s) und die Route geht erst dann weiter: Zeitverlust je Zyklus. Ein Differentialantrieb kann quer nicht korrigieren; die Frage ist, ob der Regler nahe am Ziel nachdreht (Boomerang/Carrot, Nachdrehen bei kleinem Restfehler), die Toleranz (`move_tol_in` 0,5, D-12, ADR-017) oder die Route (Ankunftspose mit passendem Kurs) angepasst wird. Zuerst messen: Wie oft laufen `goto` in den Timeout (je Schritt, 20 Läufe), und wie viel Matchzeit kostet das.

## Fertig, wenn …
- Messung vorher/nachher: Anteil `goto` mit Timeout statt Ankunft und Zeit je Zyklus (20 Läufe, Seed 1–20, Fusion an) im Ticket
- Roter Test zuerst (Regler oder Sim-Adapter mit Querversatz), Fix oder begründete Entscheidung (ADR-017-Nachtrag)
- D-12-Prüfungen (`drive_straight`, `drive_turns`) bleiben grün; Laptop-Tests, beide Roboter, `board.py validate` grün

## Kontext
lib/control/ (MotionController), sim/motion_adapter.hpp, ADR-017, D-12, D-14, SIM-46 (ADR-016-Nachtrag).

## Hardware-Prüfung
—

## Übergabe
Fertig, PR offen. Regler: Nachdrehen nahe am Ziel (`near_turn_deg`) + Mindestspannung `min_move_v` (config: 0,8 V, TODO(HW) kS).
Messung vorher: 220/506 goto im Timeout (43 %), 54 s je Lauf; nachher 0/560, Score 70,0 → 78,75 (20 Läufe, Seed 1–20, Fusion an).
Zeit je Zyklus: Stacker 9,5 → 6,9 s, Runner 7,7 → 5,7 s. Am Roboter: Pendeln an der Toleranzgrenze (0,8 V) prüfen.
Details in ADR-017 (Nachtrag SIM-48). Am Roboter prüfen: kS messen und `min_move_v` einstellen (D-07), kein Zittern am Ziel.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-15 · arjanssen (KI) · Ticket angelegt (aus KI-Review PR #127, Punkt 2) · –
2026-11-15 · arjanssen (KI) · SIM-48: Regler-Fix, roter Test zuerst, Messung vorher/nachher, ADR-017-Nachtrag · –
