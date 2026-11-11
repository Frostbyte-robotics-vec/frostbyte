# ADR-045: Spielelemente im Viewer aus dem Feld-CAD (nur Optik, lokal erzeugt)

Status: angenommen (SIM-37, 2026-11-11)

## Entscheidung
- `tools/export_field_models.py` liest das offizielle Feld-STEP (VEX 276-9250-000, 2026-04-26) und schreibt je Variante ein GLB
  (Pin ×4 Farbkombinationen, Cup, Goal rot/blau/neutral/Midfield, Loader rot/blau), `manifest.json` und `models.js` nach
  `tools/viewer/models/`. Der Viewer zeigt die Modelle, wenn `models/models.js` da ist, sonst wie bisher einfache Körper.
- **Neues Paket `cadquery` (OCP) nur für dieses Skript**, nur zum Erzeugen der Modelle: nicht im Roboter-Build, nicht in der CI.
  Der Rest der Datei und alle Tests laufen mit der Standardbibliothek; der OCP-Test wird ohne cadquery übersprungen.
  `pip install cadquery` in ein eigenes venv genügt (ca. 45 s pro Export, STEP hat 131 MB).
- **Ausgabe und STEP werden nicht committet** (`.gitignore`): das Repo ist öffentlich, die Weitergabe von VEX-CAD ist noch nicht
  geklärt (CAD-03). Wird die Lizenz geklärt, ist „GLBs committen“ nur das Entfernen einer `.gitignore`-Zeile.
- Einheiten und Achsen: das Skript rechnet mm → Zoll; Achsen wie im CAD und im Viewer (x = x_in, y oben, z = −y_in), keine
  Drehung nötig (geprüft: Goal-Positionen im CAD = field.json). Pin/Cup behalten den CAD-Ursprung (Mitte der Höhe),
  Goals stehen mit dem Boden auf y = 0, Loader sind in Spielrichtung gedreht und auf den Feldpunkt aus field.json bezogen.
- Die Modelle gehen als `models.js` (base64 in einer Skript-Datei) in den Viewer, weil `fetch`/`GLTFLoader` unter `file://` nicht
  laufen; `GLTFLoader` von jsDelivr in derselben three.js-Version (r128) wie `OrbitControls`.
- Farben: das STEP trägt nur die SolidWorks-Standardfarbe, deshalb Färbung nach Bauteilname (`color_for`). Schrauben, Muttern und
  Gewindeteile werden weggelassen (unsichtbar, viele Dreiecke).
- Dreiecksgrenzen je Art (`TRI_LIMIT`): Pin/Cup 12 000, Goal 40 000 (Midfield 70 000, nur ein Exemplar), Loader 5 000. Die
  Teile haben im CAD über 1000 Flächen (Pin 1484); unter etwa 11 000 Dreiecken kommt man ohne eigenes Dezimieren nicht. Der
  Exporter geht von fein zu grob (Abweichung 0,3 … 2,4 mm, Winkel 0,5 … 1,5 rad) bis die Grenze erfüllt ist. Netze sind je
  Variante geteilt (Klone teilen Geometrie), 120 Pins und Cups ergeben rund 1,4 Mio. Dreiecke. Wird der Viewer dadurch langsam
  (`?stats`), ist ein Dezimierungsschritt (Vertex-Clustering) das nächste Ticket.
- Der Export prüft gegen Quellen außerhalb des CAD: Höhe Pin/Cup gegen das Manual (6,5 in ± 0,05) und Goal-Oberkante gegen
  `hoehe_in` in field.json (± 0,1 in). Damit steht die Annahme „field.json und CAD passen zusammen“ im Code.

## Folgen
- Kollision, Intake und Physik der Simulation bleiben unverändert (nur `sim_params.json`); die Modelle sind reine Anzeige.
- Der Viewer kennt `variant`, `yellow_deg`, `top_color` der Feldobjekte aus `field.json` (`livesim.js` reicht sie durch).
- Die Pin-Hälften sind im CAD nach Höhe sortiert; `needsFlip` dreht ein rot/blaues Modell, damit `oben_farbe` stimmt.
- Rechnerisch ist die Cup-Höhe im Viewer jetzt 6,5 in (vorher 4,5 in geschätzt).
- Beobachtung für C-06 (nicht geändert): die Goal-Baugruppen im CAD haben unterschiedliche Unterkanten; Oberkanten passen zu
  den Werten in field.json (3,25 / 5,8 / 8,7 in).

## Beratung
Advisor-Rat vor dem Bau: Lizenz zuerst klären, solange offen nichts Abgeleitetes committen; Ausgabe in `.gitignore`, Viewer mit
Fallback; Tests ohne STEP in der CI; je Variante ein Netz. Die ursprünglichen Dreiecksziele (< 5 000 / < 20 000) waren ohne
Dezimieren nicht erreichbar, siehe oben.
