---
id: SIM-45
phase: "5"
titel: Livesim: Roboter fährt durch Pins/Cups (Räder/Antrieb außerhalb der Kollisionsform?) klären und beheben
status: todo
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

Ticket angelegt, noch nichts geändert. Im Code sichtbare Kandidaten für „fährt durch“ (ohne auf den Menschen zu warten messen): (a) `pushStanding` kehrt früh zurück, wenn ein kippendes Teil gegen seine Kipprichtung berührt wird; (b) `touchBodies` überspringt Teile mit `z > 0` (auf Goal/Stapel); (c) der Roboter weicht stehenden/kippenden Teilen nie aus (nur liegende Teile geben über `roboter_anteil` nach). Räder liegen laut Messung innerhalb des Umrisses. Danach /review #127 (SIM-46, baut auf #125 auf; #125 ist geprüft, Empfehlung mergen). Rückfrage an den Menschen steht oben (wo genau, Szene beschreiben).

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-15 · DAVIDCOPCOP (KI) · Ticket angelegt aus Handtest-Befund, Modellmaße gemessen · –
