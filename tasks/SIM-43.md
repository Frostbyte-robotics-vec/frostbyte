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

Befund des Menschen (Handtest 2026-11-15, behoben, Tests rot→grün): Pin auf einem Cup fiel beim Anfahren senkrecht durch den Cup auf den Boden, stand dort aufrecht im Cup (verschmolzen) und kippte nie. Jetzt: ein Cup, der sich weniger als die Hälfte seines Kippwinkels neigt (`TIP_HOLD`, Bauwert), trägt den Pin weiter; kippt er stärker, fällt der Pin mit der Seitengeschwindigkeit der Stelle, auf der er stand (ω · Höhe über dem Cup), landet am Boden mit Seitengeschwindigkeit stolpernd (`landSkew`: kippt in Fahrtrichtung) und, wenn er in einem Nachbarn landet, kippt er von diesem weg. Senkrechter Fall (Loslassen) bleibt aufrecht. Alter Test „Cup kippt weg: Pin steht“ geändert (er prüfte genau das beanstandete Verhalten).
Liegender Pin ohne Drehung (Mensch: Fall 1, behoben): `pushLying(i, nx, ny, v, dt)` dreht ein liegendes Teil um die Hochachse zur Schubrichtung (Achse stellt sich quer wie ein Stab an einer flachen Wand), Rate = Schubtempo/(h/2)·sin(Abweichung), keine neue Modellzahl; livesim übergibt dt. Ohne dt altes Verhalten. Test: 45° schräg geschoben → ≈ quer, quer liegend bleibt, kein Sprung. Frame-Rate-Test auf 15 ms umgestellt (3000 ms geht auf, sonst läuft der 16-ms-Lauf 8 ms länger und der noch berührte Pin dreht minimal weiter). Nicht modelliert: Pin erhöht auf dem Cup direkt vom Roboter berühren (z > 0 wird ignoriert).

Stand 2026-11-15 nach den KI-Reviews zu PR #123: Branch mit main (inkl. #124) zusammengeführt, neue Zeilen auf `*_rad` umgestellt (`grep -nE "\.(theta|omega|phi|crit)" tools/viewer/*.js` leer), `sepV` → `sep_v_in_s`, `max_top` → `max_top_in`. Doppeltes STACK: nicht reproduzierbar, gemessen (Physik: ein Teil auf einem abprallenden Teil meldet genau ein `landed` mit `stacked`; Livesim: Cup und Pin 0–60 ms nacheinander abgelegt, höchstens ein `STACK:PIN_A`), beides als Test festgehalten, kein Dedupe nötig. Der Runner-STACK-Test aus #124 braucht jetzt einen Liftbefehl (ohne Heben liegt die Hand unter der Goal-Oberkante 3,25, das Teil fällt dann zum Boden: gewollt, kein Teleport mehr). Bekannt, nicht gelöst: `vs` = Mittelpunktstempo, bei reiner Drehung 0 (Kontaktpunktgeschwindigkeit wäre richtig); `toLying` meldet kein `toppled` (Review #124). Roboter gegen liegendes Teil weiter nicht neu gemessen.
**Offen (Mensch):** was im Livesim noch falsch aussieht (Rückprall, Gleitweg, Kippdauer, Teile durch Roboter, Stapel?) und Bestätigung im Browser; danach Draft-PR aus dem Entwurf nehmen und Ticket auf review.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-12 · arjanssen (KI) · Ticket angelegt (Rest aus SIM-42, Punkt 3) · –
2026-11-15 · DAVIDCOPCOP (KI) · vier Physikpunkte mit rotem Test behoben, Feldrand Fehlalarm, Aussehen offen · –
2026-11-15 · DAVIDCOPCOP (KI) · Stapel-Regression und Sprung beim Umkippen nach Advisor behoben (rote Tests), blocked wegen Aussehen · –
2026-11-15 · matelechner01 (KI) · Review PR #123: Empfehlung ändern (Punkte siehe PR-Kommentar: sepV-Name, doppeltes STACK, Punkt 5 messen, Mensch bestätigt Aussehen) · –
2026-11-15 · arjanssen (KI) · KI-Review PR #123: Empfehlung ändern · –
2026-11-15 · DAVIDCOPCOP (KI) · Review-Punkte #123 abgearbeitet: Merge main, Umbenennungen, Tests für abprallendes Teil · –
2026-11-15 · DAVIDCOPCOP (KI) · Befund Handtest: Stapel Pin/Cup (Durchfallen, Verschmelzen, kein Kippen) behoben · –
2026-11-15 · DAVIDCOPCOP (KI) · liegender Pin dreht sich beim Schieben um die Hochachse · –
