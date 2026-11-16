---
id: SIM-45
phase: "5"
titel: Livesim: Roboter fährt durch Pins/Cups (Räder/Antrieb außerhalb der Kollisionsform?) klären und beheben
status: done
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
Entfällt (2026-11-16): Szene nicht mehr bekannt, nicht reproduzierbar; gemessenes Kippverhalten (ADR-051) ist gewollt. Bei erneutem Auftreten neues Ticket mit Szene (Taste C, stehend/liegend, Front/Seite/Ecke, Tempo).

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-15 · DAVIDCOPCOP (KI) · Ticket angelegt aus Handtest-Befund, Modellmaße gemessen · –
2026-11-15 · DAVIDCOPCOP (KI) · Frontanfahrt gemessen, im Endzustand kein Durchfahren, Rückfrage an den Menschen, Status blocked · –
2026-11-16 · arjanssen (Mensch, per KI notiert) · entfällt, nicht reproduzierbar; als done geschlossen · –
