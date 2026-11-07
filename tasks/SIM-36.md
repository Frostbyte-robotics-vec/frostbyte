---
id: SIM-36
phase: "5"
titel: Runner-Maße aus dem Fusion-Modell in sim_params.json und den Browser-Viewer übernehmen
status: in_progress
owner: arjanssen
branch: ticket/sim-36
depends: [CAD-01]
hw: nein
---

## Ziel
Das Fusion-Modell (CAD-01) ist jetzt genauer als die Schätzungen in Simulation und Viewer. Runner-Umriss, Spurbreite und die sichtbare Geometrie (Brain-Platte 0,625″ höher, zwei Tracking-Räder vorn) angleichen.

## Fertig, wenn …
- `sim/sim_params.json`, Runner: `umriss_in` = Länge 14.88 / Breite 14.77 (Startvolumen aus `pruefung.py`), `spurbreite_in` = 11.57; `quelle` = „CAD-01 (Fusion), nicht gemessen“
- `tools/viewer/robots.js`: Runner-Modell an CAD angepasst (Deckhöhe, Lage der Tracking-Räder links vorn x −2.1 / y 4.15 und rechts vorn x 1.1 / y 3.7 im CAD-System X rechts, Y vorn)
- Referenzdaten des Viewers/Livesim neu erzeugt, falls sie von den Maßen abhängen (`tools/make_livesim_reference.py`), Viewer-Tests grün
- Gleicher Seed = byte-gleiches Ergebnis; Tests hängen nicht von den Schätzwerten ab
- Laptop-Tests grün, beide Roboter bauen, `python3 tools/board.py validate` grün

## Kontext
CAD-01 (`cad/runner/`, Protokoll `letzter_lauf.txt` und `pruefung.txt`). Stacker folgt, sobald er nachgebaut ist.

## Hardware-Prüfung
—

## Übergabe
(aktueller Stand für die nächste Sitzung – wird bei jedem Sitzungsende überschrieben)

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-07 · DAVIDCOPCOP (KI) · Ticket angelegt · –
