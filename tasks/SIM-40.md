---
id: SIM-40
phase: "5"
titel: Liegende Cups/Pins im Browser-Livesim greifen: sichtbares Aufrichten in der Hand (Folge von SIM-39)
status: review
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
Stand (2026-11-12): umgesetzt, bereit für Review. Baut auf ticket/sim-39 auf (PR #115 noch offen; PR gegen ticket/sim-39). `aufricht_rad_s` (optional, Schätzwert), `hold` für liegende Teile über den Fußpunkt, `moveHeld` richtet auf, Livesim greift liegende Teile. Tests: physics_test.js 85 ok (Mutation „kein Aufrichten“ → rot), livesim_test.js 80 ok, ctest, validate. Im Browser nicht eigens angesehen (Anzeige läuft über `draw` wie SIM-38/39).
Am Rechner zu prüfen: liegenden Pin aus dem Feldplan greifen (E): richtet er sich ruhig in der Hand auf? Passt `aufricht_rad_s` (6 rad/s geschätzt)?

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-12 · arjanssen (KI) · Ticket angelegt (Lücke aus SIM-39) · –
2026-11-12 · arjanssen (KI) · Liegende Teile greifen und aufrichten, Tests, ADR-Nachtrag · siehe Branch
