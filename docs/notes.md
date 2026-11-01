# Stolperfallen und Erkenntnisse

Kurze Einträge, neueste oben: Datum · Ticket · Erkenntnis. Jede KI-Sitzung liest diese Datei beim Start eines Tickets mit.

- 2026-11-01 · SIM-07 · Viewer (tools/viewer/index.html) läuft per Doppelklick (file://): dort geht fetch nicht, Logs und field.json über Dateiauswahl/Drag-and-drop. Über `python3 -m http.server` aus dem Repo-Wurzelordner gehen auch URL-Parameter `?log=../../out/sim/x.csv&log=…&t=12&selftest`.
- 2026-11-01 · SIM-07 · JS ohne node testen: `osascript -l JavaScript datei.js` (JavaScriptCore, macOS). tools/viewer/test_logparse.py nutzt das; Browser-Selbsttest: index.html?selftest. Bild prüfen: Chrome headless `--screenshot` mit `--use-angle=swiftshader --enable-unsafe-swiftshader`.
- 2026-11-01 · SIM-07 · Heading-Konvention ist nicht festgelegt (log_format.md sagt nur „0° = rote → blaue Station“). Viewer nimmt an: +x = rot→blau, gegen den Uhrzeigersinn positiv; steht an einer Stelle (placeOnField in index.html).
