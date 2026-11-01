---
id: SIM-05
phase: "1b"
titel: Kollisionen (Roboter, Feldelemente, Rand) und Regelprüfer (Scorer: SIM-10)
status: review
owner: matelechner01
branch: ticket/sim-05
depends: [SIM-04]
hw: nein
---

## Ziel
Kollisionen (Roboter, Feldelemente, Rand) und Regelprüfer (SG2, SG3, SG6, Matchende-Halt). Der Scorer nach RSC3 wurde nach SIM-10 ausgelagert (Wertungsregeln fehlen).

## Fertig, wenn …
- je ein Testfall pro Regel (SG2, SG3, SG6, Matchende-Halt); Kollisionen Box/Box, Box/Kreis, Feldrand getestet
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün


## Kontext
Details im Plan: docs/plan.md (Ticket SIM-05). Regeln: CLAUDE.md.

## Hardware-Prüfung
—

## Übergabe
Stand: Kollisionen (sim/collision.*: Box-Box per SAT, Box-Kreis, Feldrand) und Regelprüfer (sim/rules.*: SG2, SG3, SG6, Motoren-Halt) mit Tests geschrieben, aber NICHT lokal gebaut (Toolchain fehlt) – CI entscheidet.
Offen: Scorer nach RSC3. Die Wertungsregeln (Punkte je Pin/Cup/Goal usw.) stehen nirgends im Repo; ohne Regeltext nicht raten. Frage an den Menschen: Regeltext/Tabelle der Wertung bereitstellen (docs/rules_scoring.md) und die 5 Handrechnungen liefern. Danach sim/scorer.* mit Tests. Einhängen von Kollision/Regelprüfer in run_match gehört in ein eigenes Ticket (noch anzulegen).

**Review PR #15 (arjanssen, KI): Empfehlung ändern.** (1) Merge würde SIM-05 auf done setzen, obwohl der Scorer fehlt → SIM-05 teilen (Kollisionen + Regelprüfer hier, Scorer als neues Ticket, blocked, wartet auf Wertungsregeln; plan.md + Fertig-Kriterium anpassen). (2) Status auf Branch `in_progress` vs. main `blocked` → nach Split `review`. Kleinigkeiten siehe PR-Kommentar.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-01 · matelechner01 (KI) · Kollisionen + Regelprüfer mit Tests; Scorer wartet auf Wertungsregeln
2026-11-01 · matelechner01 (KI) · Review PR #15: SIM-05 ohne Scorer (SIM-10 angelegt), Test Mittelpunkt in Box
2026-11-01 · arjanssen (KI) · Status review gesetzt
