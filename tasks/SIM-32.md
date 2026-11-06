---
id: SIM-32
phase: "5"
titel: SG3 (Höhe 50″) in der Simulation prüfen: Lifthöhe aus Winkel, RuleGuard im Match (Folge von SIM-29)
status: needs_hw
owner: matelechner01
branch: ticket/sim-32
depends: [M-11]
hw: nein
---

## Ziel
SIM-29 lässt den Lift im Match fahren, aber die Höhe ist nicht modelliert; `run_match` prüft SG3 deshalb nicht. Hier kommt die Winkel→Höhe-Abbildung der Simulation dazu (aus den gemessenen Werten von M-11, nicht geraten), und der RuleGuard (sim/rules, SG3 50″) prüft die Höhe in jedem Takt des Matches.

## Fertig, wenn …
- Winkel→Höhe je Roboter nur aus sim/sim_params.json (Messwerte aus M-11; `quelle: schaetzung` zählt nicht als Wahrheit), Tests mit eigenen Werten
- Ein Lauf, dessen Lift SG3 überschreitet, meldet `RULE:SG3` im Log; ein regelkonformer Lauf meldet nichts
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün

## Kontext
docs/decisions/ADR-009 (Nachtrag SIM-29), tasks/M-09.md, tasks/M-11.md, lib/subsystems/lift.hpp (height_for_angle), sim/rules.hpp.

## Übergabe
KI-Review PR #76 (arjanssen/KI, 6. Nov): Empfehlung: ändern. Punkte:
1. Merge-Konflikt mit main in sim/world.cpp (Schleife nach `game->step`, ~Z. 401–423; SIM-31 ist gemergt). main mergen, lösen, `test_sim_game_intake_jam` + `test_sim_height` erneut laufen lassen.
2. Fertig-Kriterium „Messwerte aus M-11“ offen (nur Schätzwerte). Mensch entscheidet: Status needs_hw statt review, oder Folgeticket „Höhentabelle mit M-11-Messwerten“ (depends M-11).
3. Frage: Stacker-Arm hebt evtl. die Oberkante; `lift_height_in` (sim/game.hpp) nutzt nur den Lift. Abdecken oder im ADR begründen.
4. ADR-009: alte Zeile „SG3 … nicht geprüft“ als überholt markieren; doppelte Leerzeile entfernen.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-05 · arjanssen (Claude) · Ticket angelegt (Folge von SIM-29, SG3 in der Simulation) · –
2026-11-05 · arjanssen (KI) · Von SIM-31 auf SIM-32 umnummeriert (Doppelvergabe mit Intake-Ticket auf main) · –
