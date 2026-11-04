---
id: SIM-24
phase: "5"
titel: target_p10 einmal je Allianz prüfen (sim_report, route_format, Routen)
status: in_progress
owner: arjanssen
branch: ticket/sim-24
depends: [SIM-22]
hw: nein
---

## Ziel
Der Spielstand ist die Allianz-Summe, `target_p10` steht aber in beiden Routen. Bericht und Format so ändern, dass das Ziel einmal je Allianz gilt. Den Zielwert legt ein Mensch fest (Frage in A-05); Ziele nie senken, damit etwas grün wird. Entstanden aus A-05.

## Fertig, wenn …
- `docs/route_format.md` und `tools/sim_report.py` beschreiben und prüfen ein Allianz-Ziel; ein ADR hält die Änderung fest
- Der höhere der bisherigen Werte (120) gilt, bis ein Mensch etwas anderes festlegt
- Tests für den Bericht (Ziel erreicht / verfehlt / Punktestand fehlt)
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün

## Kontext
tasks/A-05.md (Übergabe), ADR-026, ADR-032, docs/route_format.md.

## Hardware-Prüfung
—

## Übergabe
Implementiert auf ticket/sim-24 (ADR-034): sim_report prüft ein Allianz-Ziel (höchster target_p10), eine Meldung, Schutz gegen verschiedene Punktstände. Routen unverändert; den endgültigen Wert legt ein Mensch fest (A-05). Am Roboter zu prüfen: nichts (Auswertung am Laptop).

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-04 · arjanssen (KI) · Ticket angelegt (aus A-05) · –
2026-11-04 · arjanssen (KI) · Allianz-Ziel in sim_report, Tests, route_format, ADR-034 · ticket/sim-24
