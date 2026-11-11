---
id: SIM-37
phase: "5"
titel: Spielelemente im Viewer aus dem Feld-CAD 276-9250-000 (Pins, Cups, Goals, Loader) statt Platzhalter-Zylindern
status: review
owner: arjanssen
branch: ticket/sim-37
depends: []
hw: nein
---

## Ziel
Der Browser-Viewer (Feld und Live-Simulation) zeigt Pins, Cups, Goals und Match Loader als echte Modelle aus dem offiziellen Feld-CAD statt als Zylinder und Quader. Nur Optik: Kollision und Physik der Simulation bleiben unverändert.

## Fertig, wenn …
- `tools/export_field_models.py <STEP-Datei> <Ausgabeordner>`: liest das Feld-STEP, schreibt je Variante ein GLB (Pin ×4 Farbkombinationen, Cup, Goal rot/blau/neutral, Loader rot/blau) und `manifest.json`; Zoll, Ursprung mittig, Pivot am Boden, Achsen wie im Viewer (x = x_in, y hoch, z = −y_in); Dreiecke je Goal < 20 000, je Pin/Cup < 5 000
- Ausgabe nach `tools/viewer/models/` (in `.gitignore`, **nicht committen**, solange die Lizenzfrage offen ist, siehe CAD-03); die STEP-Datei kommt nie ins Repo
- Viewer lädt die Modelle über das Manifest, wenn vorhanden, sonst die heutigen Primitive (Fallback wie bei `HAS3D`)
- Cup-Höhe im Viewer 6,5″ (Manual), `OBJ_HEIGHT_IN` aus dem Manifest statt fest im Code
- Tests ohne STEP-Datei in der CI: mm→Zoll, Zentrierung/Pivot, Manifest-Schema (Box-Solid), Viewer-Fallback ohne Modelldatei, Pfad je Typ/Farbe mit Manifest, Maße gegen Manual (Pin/Cup 6,5″)
- ADR (oder Ergänzung zu ADR-040) für das neue Werkzeug cadquery/OCP (nur Export-Skript, nicht für Roboter-Build/CI)
- Laptop-Tests grün, beide Roboter bauen, `python3 tools/board.py validate` grün

## Kontext
Quelle: `VEX_field` (STEP AP214, SolidWorks 2024, 276-9250-000 vom 2026-04-26, mm, Y hoch). Bauteile: Pin = `276-9250-80x_*`, Cup = `276-9250-81x`, Goals = `276-9250-840_*`, Loader = `276-9250-031_*`.
Beobachtung für C-06 (hier nicht ändern): die Goal-Baugruppen haben unterschiedliche Unterkanten (Red/Blue −17,45 mm, Neutral 1/4 −80,95 mm, Neutral 0 −157,15 mm).

## Hardware-Prüfung
—

## Übergabe
Stand: angelegt.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-11 · arjanssen (KI) · Ticket angelegt · –
