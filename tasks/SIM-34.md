---
id: SIM-34
phase: "5"
titel: Umgestoßene und verschobene Feldobjekte (SIM-30) wirken auf Aufnahme und Punkte
status: todo
owner: -
branch: ticket/sim-34
depends: [SIM-30]
hw: nein
---

## Ziel
SIM-30 lässt Roboter Cups und Pins am Boden umstoßen (`TOPPLED:<id>`) und schieben, aber das Ereignismodell (SIM-04, SIM-22) kennt keine Objektpositionen: Aufnahme vom Boden und Punkte sind davon unberührt. Dieses Ticket schlägt die Brücke.

Zu entscheiden (ADR, Advisor fragen, Regelauslegung vor Annahme beim Menschen bestätigen lassen):
- Wirkung von `liegt` auf Aufnahme vom Boden (Dauer, Fehlschlagwahrscheinlichkeit; stehender Cup vs. liegender Cup/Pin).
- Aufgenommene Objekte verschwinden aus dem Kollisionsfeld (heute bleiben sie als Körper liegen).
- Verschobene Objekte: zählen Positionen für Punkte (z. B. Objekt aus der Startzone geschoben)? Nur wenn das Game Manual es verlangt.
- Anzeige der Objekte im Browser-Viewer (`live.objects()` liefert Position und Zustand).

## Fertig, wenn …
- ADR-Nachtrag mit der Entscheidung; Zahlen nur in `sim/sim_params.json`, Tests mit eigenen Fixture-Werten
- Test: Aufnahme eines umgestoßenen Objekts verhält sich wie entschieden; aufgenommenes Objekt stört die Kollision nicht mehr
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün

## Kontext
tasks/SIM-30.md, docs/decisions/ADR-025-livesim-browser.md (Nachtrag SIM-30), sim/field_objects.*, sim/objects.*, sim/game.*.

## Übergabe
Angelegt aus SIM-30 (Rat des Advisors: Brücke zu Aufnahme und Punkten als eigenes Ticket).

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-06 · DAVIDCOPCOP (KI) · Ticket angelegt · board: SIM-34 angelegt
