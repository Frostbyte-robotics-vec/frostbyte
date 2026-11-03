---
id: SIM-21
phase: "1b"
titel: Kamera: Verschieben mit gedrücktem Mausrad, R setzt die Kamera zurück
status: done
owner: arjanssen
branch: ticket/sim-21
depends: [SIM-18]
hw: nein
---

## Ziel
Die 3D-Ansicht (Viewer und /sim/) lässt sich mit gedrückter Mausrad-Taste (mittlere Maustaste) ziehen, also verschieben (Pan) wie in CAD-Programmen. Die Taste R setzt die Kamera auf die Startansicht zurück.
Heute: OrbitControls mit links = drehen, rechts = schieben, Mausrad drehen = zoomen; mittlere Taste ziehen = zoomen (Standard von three.js). R setzt im Live-Modus die Roboter zurück (index.html, keydown KeyR).

## Fertig, wenn …
- Mittlere Maustaste ziehen = verschieben (controls.mouseButtons.MIDDLE = THREE.MOUSE.PAN); linke Taste drehen, rechte Taste schieben und Mausrad-Zoom bleiben wie bisher; beim Klick mit der mittleren Taste kein Auto-Scroll des Browsers (preventDefault auf mousedown button 1 im 3D-Fenster)
- R setzt die Kamera auf die Startansicht (setView mit aktueller Ansicht: Position, controls.target = Feldmitte, Zoom), schaltet „Folgen“ aus; funktioniert im Live-Modus und in der Wiedergabe, auf /viewer/ und /sim/; gehaltene Taste wiederholt nicht
- Roboter-Zurücksetzen im Live-Modus wandert auf eine andere Taste: Vorschlag Backspace (Rücktaste); Tastenhilfe im Viewer (#liveinfo, title von #bLive, .hint) entsprechend angepasst
- R kollidiert nicht mit Eingabefeldern (wie die übrigen Tasten: nicht auslösen, wenn ein INPUT/SELECT den Fokus hat)
- Browser-Test (Playwright, wie bei SIM-18): Kamera verschieben/drehen, R drücken → cam.position und controls.target wieder wie nach dem Laden; Backspace im Live-Modus setzt die Roboter zurück, R tut das nicht mehr
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün

## Kontext
Vom Menschen gewünscht (3.11.): „Simulation mit Mausrad-Klick verschieben, R soll die Kameraposition zurücksetzen.“ Die neue Taste für das Roboter-Zurücksetzen (Backspace) ist ein Vorschlag, nicht vom Menschen festgelegt – bei Rückfrage ändern. Regeln: CLAUDE.md.

## Hardware-Prüfung
—

## Übergabe
Stand: umgesetzt in tools/viewer/index.html (gilt für /viewer/ und /sim/, eine Datei): `controls.mouseButtons` mit MIDDLE = PAN, `mousedown` mit `button === 1` ruft preventDefault (kein Auto-Scroll), R setzt die Kamera zurück (`setView` mit der aktuellen Ansicht oben/schräg, Folgen aus, `!e.repeat`, im selben keydown-Handler wie Leertaste, also in Live-Modus und Wiedergabe; Eingabefelder ausgenommen), Roboter-Zurücksetzen im Live-Modus liegt auf Backspace (Vorschlag aus dem Ticket, nicht vom Menschen festgelegt), Tastenhilfe in .hint, #liveinfo und title von #bLive angepasst. Tests: 5 neue statische Prüfungen in tools/test_sim_page.py (zuerst rot, dann grün); JS-Syntax per JavaScriptCore geprüft.
**Nicht erfüllt:** das Kriterium „Browser-Test (Playwright)“. Im Repo und auf diesem Rechner gibt es weder Playwright noch node noch einen Browser; wie bei SIM-18 prüfen die Tests nur den Quelltext. Das Verhalten im Browser (Mausrad-Taste schiebt, R setzt cam.position und controls.target zurück, Backspace setzt die Roboter zurück) ist nicht ausgeführt worden und gehört vor dem Merge von einem Menschen kurz ausprobiert; ein Playwright-Test wäre ein eigenes Ticket (Paket: ADR nötig).
Offene Frage an den Menschen: Backspace für „Roboter zurücksetzen“ okay?

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-03 · DAVIDCOPCOP (KI) · Ticket angelegt · –
2026-11-03 · arjanssen (KI) · Kamera: Mausrad-Taste schiebt, R setzt Kamera zurück, Backspace setzt Roboter zurück; statische Tests; kein Browser-Test möglich · SIM-21
