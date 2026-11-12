---
id: SIM-38
phase: "5"
titel: Physik für Spielobjekte im Browser-Livesim: Kippen, Fallen, Liegen, Ruhe (statt Sprung stehend → liegend)
status: in_progress
owner: arjanssen
branch: ticket/sim-38
depends: [SIM-30, SIM-35]
hw: nein
---

## Ziel
Im Browser-Livesim springt ein Cup/Pin beim ersten Kontakt in einem Bild von stehend auf liegend (SIM-30/35). Gewünscht: sichtbare Physik. Ein Objekt kippt um die Bodenkante, fällt unter Schwerkraft, schlägt auf, prallt leicht zurück, rollt/gleitet aus und kommt zur Ruhe; Objekte stoßen sich gegenseitig. Neues, DOM-freies Modul `tools/viewer/physics.js` (feste Schritte, deterministisch, keine Physik-Bibliothek), eingebunden in `livesim.js`; der Viewer zeichnet die Lage aus der Physik. Greifen, Ablegen und Stapeln folgen in SIM-39.

## Fertig, wenn …
- Ohne Block `physik` in den Parametern läuft `livesim.js` exakt wie bisher (bestehende Tests, `test_livesim.py` und die C++-Referenz bleiben unverändert grün)
- Block `physik` in sim/sim_params.json (Schwerpunkt, Rückprall, Reibung, Objekt-Objekt-Stoß, Ruheschwellen; je mit `quelle: schaetzung`); keine Zahl im JS
- Kippen als Pendel um die Bodenkante: Roboter drückt kinematisch, über dem Kippwinkel `atan(2r/h)` übernimmt die Schwerkraft; darunter richtet sich das Objekt wieder auf; Aufschlag bei 90° mit Rückprall und Ausschwingen
- Liegende Objekte gleiten/rollen mit Reibung aus und bleiben liegen (kein Zittern, nie unter z = 0); Objekt gegen Objekt (Kreis gegen Kreis)
- Gleiche Eingabe ergibt bei verschiedenen Bildraten denselben Zustand
- Der Viewer zeichnet Höhe, Kipprichtung und Winkel aus der Physik (Drehung um die Kante, nicht um den Mittelpunkt); kein Sprung mehr
- Tests mit eigenen Fixture-Werten, analytisch prüfbar (Fallzeit √(2h/g), Energiebilanz beim Aufschlag, Aufrichten unter dem Kippwinkel, Ruhe, Bildraten-Unabhängigkeit)
- ADR-051 (Entscheidung, Abweichung vom C++-ObjectField, Rat des Advisors); Laptop-Tests grün, beide Roboter bauen, `python3 tools/board.py validate` grün

## Kontext
docs/decisions/ADR-025-livesim-browser.md (Nachträge SIM-30, SIM-34), sim/field_objects.*, tools/viewer/livesim.js, tools/viewer/index.html (updateObjects). Die C++-Simulation bleibt ereignisbasiert (ADR-026) und bekommt die Physik nicht; der Livesim ist Vorführung, keine Quelle für Messwerte.

## Hardware-Prüfung
—

## Übergabe
Stand (2026-11-12): umgesetzt, bereit für Review. physics.js (Kippen, Fallen, Liegen, Stoß), Einbindung in livesim.js (nur mit Block `physik`), Anzeige in index.html (`o.draw`), Block `physik` in sim_params.json (Schätzwerte), physics_test.js (44 Prüfungen, analytisch; Mutationstest: Rückprall und Drehmoment geändert → Tests rot), ADR-051, CI/Dashboard kopieren physics.js. Rat des Advisors im ADR.
Lokal geprüft: Tests unter jsc (kein node hier): 44 + 80 ok; ctest grün; board validate grün. Nicht geprüft: Roboter-Builds (kein PROS), test_livesim.py unter node (hier übersprungen, CI führt es aus), **Anzeige im Browser** (headless-Chrome-Steuerung wurde nicht erlaubt).
Am Rechner zu prüfen: index.html über http öffnen, Live starten, in einen Cup/Pin fahren: kippt er sichtbar um die Kante, prallt, bleibt liegen? Liegende Pins aus dem Feldplan: gelbe Seite richtig? Werte in `physik` (Rückprall, Reibung, kontakt_hoehe_in) nach Gefühl nachstellen.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-12 · arjanssen (KI) · Ticket angelegt (Wunsch des Menschen: sichtbare Physik und Stapeln) · –
2026-11-12 · arjanssen (KI) · Physik, Livesim-Einbindung, Anzeige, Tests, ADR-051 · siehe Branch
