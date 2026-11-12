---
id: SIM-39
phase: "5"
titel: Greifen, Ablegen und Stapeln im Browser-Livesim (Cups/Pins aufnehmen, auf Goals und Stapel setzen, Fall auf die Stapelspitze)
status: in_progress
owner: arjanssen
branch: ticket/sim-39
depends: [SIM-38]
hw: nein
---

## Ziel
Aufbauend auf der Physik von SIM-38: Der Roboter im Livesim kann ein Objekt greifen (Besitz höchstens 1 Pin + 1 Cup, SG6), mit dem Lift heben, vor einem Goal oder Stapel ablegen. Das Objekt fällt auf die höchste Stützfläche (Boden, Goal, Stapelspitze), rastet auf der Goal-Mitte ein und bleibt sichtbar gestapelt; ein schlecht gesetzter Stapel kippt.

## Fertig, wenn …
- Tasten fürs Greifen/Ablegen/Heben (vorher index.html komplett auf freie Tasten prüfen) und gleiche Schnittstelle in `livesim.js` für Tests
- SG6: ein zweites Teil derselben Art wird abgelehnt und als Ereignis geloggt (RuleGuard nicht umgehen)
- Greifreichweite wie `ObjectField::nearest_pickup` (vergrößerter Umriss, nächstes Objekt)
- Ablegen über einem Goal landet auf `hoehe_in` (field.json) bzw. der Stapelspitze; Stufenhöhe aus der Objekthöhe (`ebenen.stufe_in` ist null, C-06) als Schätzung in `physik`, nicht in field.json
- Tests mit Fixture-Werten (Fall auf Goal, Stapel mehrerer Teile, SG6-Ablehnung, Fangradius)
- Viewer zeichnet gehaltene und gestapelte Objekte; ADR-051-Nachtrag; Laptop-Tests grün, Roboter bauen, `board.py validate` grün

## Kontext
tasks/SIM-38.md, docs/decisions/ADR-026 und ADR-024 (Stapelregeln), docs/rules_scoring.md, tasks/C-06.md (Stufenhöhe offen).

## Hardware-Prüfung
—

## Übergabe
Noch nicht begonnen.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-12 · arjanssen (KI) · Ticket angelegt (Folge von SIM-38) · –
