---
id: SIM-35
phase: "5"
titel: Feldobjekte (Cups/Pins) im Browser-Viewer zeichnen
status: review
owner: arjanssen
branch: ticket/sim-35
depends: [SIM-30]
hw: nein
---

## Ziel
`live.objects()` (livesim.js, SIM-30) liefert Position und Zustand der Feldobjekte, der Viewer zeichnet sie aber nicht. Cups und Pins als Kreise auf dem Feld zeigen (stehend/liegend unterscheidbar), Verschiebung und Umstoßen live sichtbar.

## Fertig, wenn …
- Viewer zeichnet alle Objekte aus `live.objects()`; stehend und liegend sind unterscheidbar
- Test in `tools/viewer/livesim_test.js` (oder passender Viewer-Test) für die Zeichendaten, soweit ohne Browser prüfbar
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün

## Kontext
docs/decisions/ADR-025-livesim-browser.md (Nachträge SIM-30, SIM-34), tools/viewer/livesim.js, tools/viewer/index.html.

## Übergabe
Stand (2026-11-06): umgesetzt. index.html: updateObjects() zeichnet live.objects() (Cup/Pin als Zylinder, stehend aufrecht, liegend auf der Seite), livesim.js liefert dafür `kind`. Test: livesim_test.js (kind). Lokal ohne node mit jsc (macOS) ausgeführt: 80 ok, 0 Fehler; die Inline-Skripte von index.html auf Syntax geprüft. Das Zeichnen selbst wurde NICHT im Browser gesehen.
Am Rechner zu prüfen: index.html im Browser öffnen, Live starten, in einen Cup fahren: er kippt, wird geschoben; Farben/Größen passen? (Höhen 4,5/6,5 in sind Anzeigeschätzung.)

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-06 · arjanssen (KI) · Ticket angelegt · –
2026-11-06 · arjanssen (KI) · Objekte im Viewer gezeichnet, kind in livesim.js, Test · –
