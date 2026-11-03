---
id: SIM-21
phase: "1b"
titel: Kamera: Verschieben mit gedrücktem Mausrad, R setzt die Kamera zurück
status: todo
owner:
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
—

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-03 · DAVIDCOPCOP (KI) · Ticket angelegt · –
