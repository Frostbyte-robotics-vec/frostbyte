---
id: SIM-14
phase: "1b"
titel: Live-Handsteuerung im Browser mit WASD (Fahrmodell aus sim_params.json)
status: review
owner: DAVIDCOPCOP
branch: ticket/sim-14
depends: [SIM-13]
hw: nein
---

## Ziel
Auf der Seite /sim/ gibt es einen Modus „Live“: Ein Roboter wird mit der Tastatur gefahren, die Simulation läuft im Browser in Echtzeit. Vorläufige Steuerung für Tests und Vorführung, kein Ersatz für frostbyte_sim.
- W/S: vor/zurück, A/D: drehen (Arcade wie Fahrerbedienung in docs/controls.md); Shift gehalten = Langsam-Modus
- Tab: zwischen Stacker und Runner wechseln (der andere steht still); R: beide auf Startpose zurücksetzen
- Startposen aus field/field.json (null-Koordinaten → feste Ersatzpose, im Log als Hinweis vermerkt)

## Fertig, wenn …
- Fahrmodell als eigene Datei tools/viewer/livesim.js ohne DOM-Zugriff: Differentialantrieb mit kS/kV/kA, spurbreite_in, motor_max_V je Roboter aus sim/sim_params.json (keine Modellzahlen im JS), Schritt schritt_ms, Steuerung alle steuer_ms
- Tastenzustand → Motorspannungen nur in einer Funktion (gut austauschbar gegen Gamepad später)
- Node-Test tools/viewer/livesim_test.js (wie logparse_test.js): Geradeausfahrt erreicht die stationäre Geschwindigkeit aus kV/kS, Drehung auf der Stelle ändert x/y nicht, gleicher Tastenverlauf = gleiche Posen; läuft in der CI
- Steuertakt hängt nicht an der Bildrate (feste Schritte, Akkumulator), Tab im Hintergrund → Pause
- ADR in docs/decisions/ (nächste freie Nummer): Fahrmodell im Browser als vereinfachte Kopie des C++-Modells, warum kein WebAssembly jetzt, Abgleich folgt in SIM-17
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün

## Kontext
Vom Menschen gewünscht (3.11.): manuelle Grundsimulation bis 4.11. Modellwerte mit "quelle": "schaetzung" sind Platzhalter – Tests prüfen Verhalten relativ zu den Parametern, nicht feste Zahlen. Vorbild: sim/drive.cpp. Regeln: CLAUDE.md.

## Hardware-Prüfung
—

## Übergabe
Stand: alles umgesetzt (livesim.js, Tests, Live-Modus in index.html, ADR-025, CI/Dashboard kopieren livesim.js + sim_params.json). Python-Tests, Format, Board validate, ctest grün. Roboter-Build lokal nicht möglich (ARM-Toolchain: „Cannot create temporary file in C:\WINDOWS\“, Umgebungsproblem, kein C++ geändert) – CI prüft. Nicht im echten Browser ausprobiert (nur `node --check` des Inline-Skripts): bitte /sim/ öffnen, „Live“ drücken, WASD/Tab/R/Shift testen.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-03 · DAVIDCOPCOP (KI) · Ticket angelegt · –
2026-11-03 · DAVIDCOPCOP (KI) · livesim.js, Tests (Node), live-Block in sim_params.json · –
2026-11-03 · DAVIDCOPCOP (KI) · Live-Modus in index.html, ADR-025, Workflows · –
