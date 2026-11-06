---
id: SIM-35
phase: "5"
titel: Feldobjekte (Cups/Pins) im Browser-Viewer zeichnen
status: in_progress
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
Angelegt aus SIM-34 (Anzeige war dort im „Zu entscheiden“, aber nicht in „Fertig, wenn“; livesim.js kennt keine Aufnahme, nur Kollision).

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-06 · arjanssen (KI) · Ticket angelegt · –
