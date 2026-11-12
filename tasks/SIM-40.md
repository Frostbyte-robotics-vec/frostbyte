---
id: SIM-40
phase: "5"
titel: Liegende Cups/Pins im Browser-Livesim greifen: sichtbares Aufrichten in der Hand (Folge von SIM-39)
status: in_progress
owner: arjanssen
branch: ticket/sim-40
depends: [SIM-39]
hw: nein
---

## Ziel
SIM-39 greift nur stehende Teile. 16 Pins liegen von Anfang an, und jedes umgestoßene Objekt liegt; sie sollen greifbar sein. Beim Greifen richtet sich das Teil mit endlicher Winkelgeschwindigkeit in der Hand auf (kein Klappen in einem Bild).

## Fertig, wenn …
- Ein liegendes Teil vor dem Roboter wird gegriffen (gleiche Reichweite und SG6 wie bei stehenden); Greifen setzt es nicht schon in die Hand, sondern zieht es hin und richtet es mit `physik.greifen.aufricht_rad_s` auf
- Der Fußpunkt springt dabei nie (Stetigkeitstest wie in SIM-38), der Winkel nimmt monoton ab, am Ende steht es senkrecht in der Hand
- Losgelassen fällt und steht es wie jedes gehaltene Teil, Stapeln klappt danach
- Ohne `aufricht_rad_s` im Block bleibt das Greifen liegender Teile aus (Altverhalten); Zahl nur in sim/sim_params.json (`quelle: schaetzung`), Tests mit Fixture-Werten
- Viewer zeichnet das aufrichtende Teil aus `draw` (keine Änderung nötig, wenn `draw` stimmt); ADR-051-Nachtrag; Laptop-Tests grün, `board.py validate` grün

## Kontext
tasks/SIM-39.md, docs/decisions/ADR-051 (Nachtrag SIM-39: „liegende Objekte werden nicht gegriffen“), tools/viewer/physics.js.

## Hardware-Prüfung
—

## Übergabe
Noch nicht begonnen.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-12 · arjanssen (KI) · Ticket angelegt (Lücke aus SIM-39) · –
