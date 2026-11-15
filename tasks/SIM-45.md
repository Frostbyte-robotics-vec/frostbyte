---
id: SIM-45
phase: "5"
titel: Livesim: Roboter fährt durch Pins/Cups (Räder/Antrieb außerhalb der Kollisionsform?) klären und beheben
status: blocked
owner: 
branch: 
depends: [SIM-43]
hw: nein
---

## Ziel
Befund des Menschen (Handtest 2026-11-15, nach SIM-43): „Es gibt keine Kollision mit den äußeren Teilen des größeren Antriebs (Stacker) mit Pins oder Cups, der Roboter fährt einfach durch.“ Auf Rückfrage: gemeint sind die Räder/der Antrieb.

Messung (CAD-Modell tools/viewer/robots.js, Box3 je Ebene, Stacker, Modell-Zoll, Front = −z): Schicht `antrieb` x ±7,53, z −8,03…8,03, y 0…4,0, also **innerhalb** des Kollisionsumrisses 17,5 × 17,5 (`roboter.stacker.umriss_in`); Rahmen/Struktur ±8,75. Nur Lift, Greifer und Aufnahme ragen weit darüber hinaus (Gesamtmodell z −18,8…+12,25). Die Räder allein erklären das Durchfahren also nicht.

Zu klären (zuerst messen, dann roter Test):
- Wo genau fährt der Roboter durch? (Pin/Cup stehend oder liegend, Seite/Ecke/Front, Tempo; Umriss mit Taste C einblenden und prüfen, ob er mit dem Modell deckt)
- Kippende Teile (`pushStanding`): der Roboter weicht nicht zurück, der Pin neigt sich nur so weit, wie die Eindringtiefe es erzwingt; das Modell läuft dabei sichtbar durch den Roboter, bis er liegt (gewollt in SIM-38, ADR-051). Ist das der Eindruck „fährt durch“?
- Ausgreifende Teile (Lift/Greifer/Aufnahme vorn und hinten) haben keine Kollisionsform; nötig, wenn Pins/Cups damit berührt werden sollen (Höhenabhängig, Maße nur aus dem CAD, `schaetzung`).

## Fertig, wenn …
Je Beobachtung: Messwert vorher/nachher, roter Test, Fix oder Begründung „gewollt“/„Fehlalarm“; Laptop-Tests grün, `board.py validate` grün; der Mensch bestätigt im Browser, dass der Roboter keine Teile mehr durchfährt.

## Kontext
tools/viewer/livesim.js (`boxOf`, `touchBodies`, `penetrationBoxCircle`), tools/viewer/physics.js (`pushStanding`), tools/viewer/robots.js, ADR-051, SIM-38, SIM-43

## Hardware-Prüfung
—

## Übergabe
(aktueller Stand für die nächste Sitzung – wird bei jedem Sitzungsende überschrieben)

Gemessen (Livesim-Fixture wie physics_test Abschnitt 14, Stacker-Umriss 17,5 × 17,5, Teilradius 1,58, Frontanfahrt; Skript nur im Scratchpad, nicht im Repo):
- Cup/Pin stehend, Frontanfahrt 3 / 6 / 11 V: Teil durchläuft standing → tipping → lying und liegt danach VOR dem Roboter (Cup bei 6 V: Roboterfront −1,85 in, Objektmitte +0,14 in = Front + Radius, es wird geschoben, nicht überfahren). Im Endzustand kein Durchfahren.
- Während des Kippens (rund 0,3 s) steckt die Teilmitte bis 6 in hinter der Roboterfront (Pin 5,6 · Cup 6,1 in bei 6 V, bei 11 V gleich). Folgt aus Berührhöhe 4 in plus Hebel um die Fußkante und ist in SIM-38/ADR-051 so gewollt; sichtbar als „Modell läuft kurz durch den Roboter“.
- Liegender Pin: kein Durchfahren (SIM-30 testet das).
- Räder liegen laut CAD-Messung innerhalb des Umrisses. Lift/Greifer/Aufnahme haben keine Kollisionsform (Kandidat d).
Es ist kein Fehler reproduzierbar. Ohne die Szene des Menschen rate ich nicht weiter (Status blocked).

Frage an den Menschen: Szene beschreiben. (1) Mit Taste C: deckt der Umriss die Räder? (2) Cup/Pin stehend oder liegend, am Boden oder auf Goal/Stapel? (3) Front, Seite, Ecke oder beim Drehen auf der Stelle? (4) Tempo (Shift)? Bekannte Lücken: beim Drehen ist `vs` = 0, also Winkelgeschwindigkeit 0; Kandidat (b): Teile mit z > 0 werden nie berührt.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-15 · DAVIDCOPCOP (KI) · Ticket angelegt aus Handtest-Befund, Modellmaße gemessen · –
2026-11-15 · DAVIDCOPCOP (KI) · Frontanfahrt gemessen, im Endzustand kein Durchfahren, Rückfrage an den Menschen, Status blocked · –
