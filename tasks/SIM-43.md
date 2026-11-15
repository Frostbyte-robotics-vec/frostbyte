---
id: SIM-43
phase: "5"
titel: Livesim-Physik: restliche Auffälligkeiten (Kippen, Überlappung, Stapel) mit Messwerten beheben
status: in_progress
owner: DAVIDCOPCOP
branch: ticket/sim-43
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
Stand 2026-11-15: vier der fünf Punkte erledigt (Prüfstand = Node-Skript mit physics.js, kein Chrome; Messwerte vorher → nachher):
- `pushStanding`: Sprung der Eindringtiefe um 0,3 in bei 1 ms → omega 73 rad/s (Fahrt mit 60 in/s ergibt sonst nur 15–17 rad/s). Neuer optionaler Parameter `v_in_s`, omega ≤ kick·v/Berührhöhe; livesim übergibt die Fahrgeschwindigkeit entlang der Schubrichtung. Ohne Parameter wie vorher.
- `toLying` ohne Überlappungsauflösung: Pin kippt auf Nachbarn zu, Abstand 0,75 in bei r1+r2 = 3 → jetzt ≥ 3. Feldrand: **Fehlalarm** (clampField hielt ihn schon). Roboter/Wand wirkt weiter über `touchBodies` (Ausweichen), nicht getestet.
- `settle()`: zwei gleich hoch deklarierte Teile am selben Ort ergaben 8/12 statt 4/4 (Teil trug Teil); jetzt zählt eine Unterlage nur, wenn sie schon gesetzt ist und niedriger deklariert war.
- Loslassen unter der Goal-Oberkante: z sprang von 1,0 auf 3,25 (Teleport); jetzt zählen nur Unterlagen, deren Oberkante nicht über der Höhe des Teils im Schritt davor liegt (`max_top` in `supportAt`).
Stolperfalle: der alte Test „16 Stapel / drei Teile“ stapelte schon am Start (alle drei auf (0, 0)) und bestand nur durch den Teleport; Startplätze getrennt. Offen als Beobachtung: fällt ein Teil durch ein gerade abprallendes (state falling) Teil darunter, landet es darunter statt darauf (vorher kaschiert der Teleport das); im Livesim wegen der Sekunden zwischen den Ablagen kaum relevant.
**Offen:** Aussehen nach Gefühl (Rückprall, Reibung, Kippdauer): braucht die Beschreibung des Menschen, was konkret falsch aussieht, und Bestätigung im Browser (tools/viewer/index.html, Livesim). Test: `py -3 -m unittest discover -s tools/viewer -p test_livesim.py`.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-12 · arjanssen (KI) · Ticket angelegt (Rest aus SIM-42, Punkt 3) · –
2026-11-15 · DAVIDCOPCOP (KI) · vier Physikpunkte mit rotem Test behoben, Feldrand Fehlalarm, Aussehen offen · –
