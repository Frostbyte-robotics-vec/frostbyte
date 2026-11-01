---
id: SIM-05
phase: "1b"
titel: Kollisionen (Roboter, Feldelemente, Rand), Regelprüfer, Scorer nach RSC3
status: blocked
owner: matelechner01
branch: ticket/sim-05
depends: [SIM-04]
hw: nein
---

## Ziel
Kollisionen (Roboter, Feldelemente, Rand), Regelprüfer, Scorer nach RSC3

## Fertig, wenn …
- je ein Testfall pro Regel; Scorer stimmt mit 5 Handrechnungen überein
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün


## Kontext
Details im Plan: docs/plan.md (Ticket SIM-05). Regeln: CLAUDE.md.

## Hardware-Prüfung
—

## Übergabe
(aktueller Stand für die nächste Sitzung – wird bei jedem Sitzungsende überschrieben)

**Review PR #15 (arjanssen, KI): Empfehlung ändern.** (1) Merge würde SIM-05 auf done setzen, obwohl der Scorer fehlt → SIM-05 teilen (Kollisionen + Regelprüfer hier, Scorer als neues Ticket, blocked, wartet auf Wertungsregeln; plan.md + Fertig-Kriterium anpassen). (2) Status auf Branch `in_progress` vs. main `blocked` → nach Split `review`. Kleinigkeiten siehe PR-Kommentar.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
