---
id: SIM-43
phase: "5"
titel: Livesim-Physik: restliche Auffälligkeiten (Kippen, Überlappung, Stapel) mit Messwerten beheben
status: blocked
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
Stand 2026-11-15 (blocked, wartet auf den Menschen): vier der fünf Punkte erledigt, Prüfstand = Node-Skript mit physics.js (kein Chrome); Werte vorher → nachher:
- `pushStanding` (Fahrt 60 in/s, Eindringtiefe springt in 1 ms um 0,3 in): omega 73,5 → 17,1 rad/s (ohne Sprung 16,9, = v/Berührhöhe); Winkelsprung je ms 0,146 → 0,088 rad. Neuer optionaler Parameter `v_in_s`, omega ≤ kick·v/Berührhöhe; livesim übergibt die Fahrgeschwindigkeit entlang der Schubrichtung, ohne Parameter wie vorher. θ springt weiter auf den erzwungenen Winkel: das ist geometrisch gewollt (der Roboter steckt schon so tief im Standkreis, Kippen ist die Folge der Eindringtiefe), nur omega war überhöht.
- `toLying` ohne Überlappungsauflösung: Pin kippt auf Nachbarn zu, Abstand 0,75 in bei r1+r2 = 3 → ≥ 3. Auflösung als Geschwindigkeit über `ausweich_s` (`sepV`, kein Sprung; Lageänderung je ms < 0,5 in getestet). Feldrand: **Fehlalarm** (clampField hielt ihn schon). Roboter gegen liegendes Teil: wirkt weiter über `touchBodies`/`ausweich_s` (SIM-30), nicht neu gemessen → ausdrücklich offen.
- `settle()`: zwei gleich hoch deklarierte Teile am selben Ort ergaben 8/12 statt 4/4; jetzt zählt eine Unterlage nur, wenn schon gesetzt und niedriger deklariert.
- Loslassen unter der Goal-Oberkante: z sprang 1,0 → 3,25 (Teleport); jetzt zählen nur Unterlagen, deren Oberkante nicht über der Höhe im Schritt davor liegt (`max_top`).
- Folge davon (Advisor-Hinweis, mit rotem Test bestätigt): der alte Stapel-Test bestand nur durch den Teleport; ohne ihn fiel ein Teil durch ein noch abprallendes (falling) Teil darunter. Jetzt zählt für ein fallendes Teil auch ein darunter fallendes Teil als Unterlage; die Startplätze im alten Test sind getrennt (sie standen am Start schon aufeinander) und der Fall hat einen eigenen Test. Beobachtung: ein Teil kann dadurch kurz auf einem abprallenden Teil „landen“ und beim Landen des unteren nochmals (zweites `landed`/STACK möglich).
Rat des Advisors (vor /finish): nicht `review`, sondern `blocked`, weil „Aussehen nach Gefühl“ und die Bestätigung im Browser ein Mensch klären muss; Tests nicht abschwächen.
**Frage an den Menschen:** Was genau sieht im Livesim (tools/viewer/index.html) noch falsch aus: Rückprall zu stark/schwach, Gleitweg, Kippdauer, Teile durch Roboter, Stapel? Bitte Szene beschreiben. Danach Werte in `physik` (sim_params.json, alles Schätzungen) anpassen und im Browser bestätigen lassen; dann kann das Ticket auf review.
Test: `py -3 -m unittest discover -s tools/viewer -p "test_*.py"`. clang-format war auf diesem Rechner nicht installiert (nur JS geändert).

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-12 · arjanssen (KI) · Ticket angelegt (Rest aus SIM-42, Punkt 3) · –
2026-11-15 · DAVIDCOPCOP (KI) · vier Physikpunkte mit rotem Test behoben, Feldrand Fehlalarm, Aussehen offen · –
2026-11-15 · DAVIDCOPCOP (KI) · Stapel-Regression und Sprung beim Umkippen nach Advisor behoben (rote Tests), blocked wegen Aussehen · –
