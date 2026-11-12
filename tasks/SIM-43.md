---
id: SIM-43
phase: "5"
titel: Livesim-Physik: restliche Auffälligkeiten (Kippen, Überlappung, Stapel) mit Messwerten beheben
status: todo
owner:
branch:
depends: [SIM-42]
hw: nein
---

## Ziel
Der Mensch fand im Handtest (2026-11-12) die Physik „noch nicht gut“. SIM-42 hat nur `release()` repariert. Hier der Rest, jeweils zuerst messen (Prüfstand: Chrome per DevTools-Protokoll, pro Bild `maxJump`, Überlappungstiefe, `omega`), dann roter Test, dann Fix:
- Roboter mit vollem Tempo in einen stehenden Cup: Spitze von `omega` und `maxJump` (SIM-41: `pushStanding`, Winkelsprung)
- Nach dem Umkippen: Überlappung von Teil und Roboter, Wand, Nachbar (SIM-41: `toLying` ohne Überlappungsauflösung)
- `settle()`: gleich hohe Teile zählen als Unterlage (Stapel schwebt)
- Loslassen unterhalb der Auflage teleportiert auf die Auflage (ADR: „kein Teleport“)
- Aussehen nach Gefühl (Rückprall, Reibung, Kippdauer): **Beschreibung des Menschen nötig**, was konkret falsch aussieht; Werte in `physik` sind Schätzungen

## Fertig, wenn …
Je Punkt: Messwert vorher/nachher, roter Test, Fix oder Begründung „Fehlalarm“; Laptop-Tests grün, `board.py validate` grün; der Mensch bestätigt das Aussehen im Browser.

## Kontext
tools/viewer/physics.js, tools/viewer/livesim.js, ADR-051, SIM-41, SIM-42

## Hardware-Prüfung
—

## Übergabe
(aktueller Stand für die nächste Sitzung – wird bei jedem Sitzungsende überschrieben)

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-12 · arjanssen (KI) · Ticket angelegt (Rest aus SIM-42, Punkt 3) · –
