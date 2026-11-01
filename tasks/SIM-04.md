---
id: SIM-04
phase: "1b"
titel: Mechanismen als Gelenke + Objektmodell als Ereignisse (Loader, Boden, Ausrichtung, Greifen, Setzen)
status: review
owner: DAVIDCOPCOP
branch: ticket/sim-04
depends: [SIM-01]
hw: nein
---

## Ziel
Mechanismen als Gelenke + Objektmodell als Ereignisse (Loader, Boden, Ausrichtung, Greifen, Setzen)

## Fertig, wenn …
- ein Zyklus läuft im Sim durch; Fehlerquote ± 2 % über 1000 Versuche
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün


## Kontext
Details im Plan: docs/plan.md (Ticket SIM-04). Regeln: CLAUDE.md.

## Hardware-Prüfung
—

## Übergabe
Stand: Gelenke (Lift/Arm/Greifer) und Objektereignisse samt Zyklus fertig und getestet (sim/mechanism.*, sim/objects.*, test/test_sim_mechanism.cpp, ADR-009). Nicht in run_match eingehängt (braucht Route/Fahrmodell).

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-01 · DAVIDCOPCOP · Gelenke, Objektereignisse, Zyklus, ADR-009
2026-11-01 · DAVIDCOPCOP · Review-Fixes: echte Toleranzen statt Approx.epsilon, diskrete Bremskurve
