---
id: SIM-37
phase: "5"
titel: Spielelemente im Viewer aus dem Feld-CAD 276-9250-000 (Pins, Cups, Goals, Loader) statt Platzhalter-Zylindern
status: in_progress
owner: arjanssen
branch: ticket/sim-37
depends: []
hw: nein
---

## Ziel
Der Browser-Viewer (Feld und Live-Simulation) zeigt Pins, Cups, Goals und Match Loader als echte Modelle aus dem offiziellen Feld-CAD statt als Zylinder und Quader. Nur Optik: Kollision und Physik der Simulation bleiben unverändert.

## Fertig, wenn …
- `tools/export_field_models.py <STEP-Datei> [--out …]`: liest das Feld-STEP, schreibt je Variante ein GLB (Pin ×4 Farbkombinationen, Cup, Goal rot/blau/neutral, Loader rot/blau) und `manifest.json`; Zoll, Ursprung mittig, Pivot am Boden, Achsen wie im Viewer (x = x_in, y hoch, z = −y_in); Dreiecke je Goal ≤ 40 000 (Midfield ≤ 70 000), je Pin/Cup ≤ 12 000, je Loader ≤ 5 000 (unter ~11 000 geht es ohne eigenes Dezimieren nicht, ADR-045)
- Ausgabe nach `tools/viewer/models/` (`models.js` und `manifest.json` im Repo, da VEX die Weitergabe am 2026-11-11 mündlich bestätigt hat, ADR-045; `*.glb` gitignored); die STEP-Datei kommt nie ins Repo
- Viewer lädt die Modelle über das Manifest, wenn vorhanden, sonst die heutigen Primitive (Fallback wie bei `HAS3D`)
- Cup-Höhe im Viewer 6,5″ (Manual), `OBJ_HEIGHT_IN` aus dem Manifest statt fest im Code
- Tests ohne STEP-Datei in der CI: mm→Zoll, Zentrierung/Pivot, Manifest-Schema (Box-Solid), Viewer-Fallback ohne Modelldatei, Pfad je Typ/Farbe mit Manifest, Maße gegen Manual (Pin/Cup 6,5″)
- ADR-045 für das neue Werkzeug cadquery/OCP (nur Export-Skript, nicht für Roboter-Build/CI)
- Laptop-Tests grün, beide Roboter bauen, `python3 tools/board.py validate` grün

## Kontext
Quelle: `VEX_field` (STEP AP214, SolidWorks 2024, 276-9250-000 vom 2026-04-26, mm, Y hoch). Bauteile: Pin = `276-9250-80x_*`, Cup = `276-9250-81x`, Goals = `276-9250-840_*`, Loader = `276-9250-031_*`.
Beobachtung für C-06 (hier nicht ändern): die Goal-Baugruppen haben unterschiedliche Unterkanten (Red/Blue −17,45 mm, Neutral 1/4 −80,95 mm, Neutral 0 −157,15 mm).

## Hardware-Prüfung
—

## Übergabe
Stand: fertig, PR offen. Exporter, Viewer-Anbindung, Tests, ADR-045. Mit dem echten Feld-STEP (VEX_field, 131 MB) erzeugt und im Browser
geprüft: Goals, Pins (4 Varianten, Hälften farbig), Cups, Loader stehen an den Feldpunkten, Live-Modus zeigt Pins/Cups als Modell.
Modelle erzeugen: venv mit `pip install cadquery`, dann `python3 tools/export_field_models.py <STEP>` (ca. 1 min); Ausgabe in
tools/viewer/models/ (gitignored). Ohne die Dateien zeichnet der Viewer wie vorher.
Lizenz: Schweizer VEX-Vertreter hat mündlich zugestimmt (arjanssen, 2026-11-11); models.js/manifest.json sind eingecheckt, der Dashboard-Workflow
kopiert fieldmodels.js und models/ auf die Webseite. Offen für Menschen: (2) Optisch prüfen: Cup-Lage (klare Hälfte unten),
Loader-Ausrichtung (aus dem CAD übernommen), liegende Pins (gelb_deg). (3) Leistung: ?stats im Viewer ansehen (ca. 1,4 Mio. Dreiecke bei
120 Pins/Cups); wenn es ruckelt, neues Ticket „Dezimieren“. (4) Für C-06: Goal-Unterkanten im CAD unterschiedlich (Red/Blue −17,45 mm,
Neutral 1/4 −80,95 mm, Neutral 0 −157,15 mm relativ zum Modellursprung); Oberkanten passen zu field.json.
Bekannte Grenze: stehende Pins zeigen die CAD-Lage (gelb unten); pin_yellow_up aus Log/sim_params fließt nicht in die Anzeige, weil live.objects() es je Objekt nicht führt.
Nicht geändert: Kollision/Physik, field.json, sim_params.json. Cup-Höhe im Viewer 4,5 → 6,5 in (Manual).
Am Roboter nichts zu prüfen (nur Anzeige).

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-11 · arjanssen (KI) · Ticket angelegt · –
2026-11-11 · arjanssen (KI) · Exporter, Viewer-Modelle, Tests, ADR-045; mit echtem STEP geprüft · siehe Branch
2026-11-11 · arjanssen (KI) · VEX-Freigabe (mündlich) eingetragen, models.js/manifest.json eingecheckt, Dashboard-Workflow kopiert Modelle · siehe Branch
