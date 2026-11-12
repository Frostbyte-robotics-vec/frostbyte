---
id: DOC-02
phase: "5"
titel: Offene Fragen an den Menschen sammeln und beantworten (M-08, C-06, SIM-33)
status: blocked
owner:
branch:
depends: []
hw: nein
---

## Ziel
Drei blockierte Tickets warten nur auf Antworten, die die KI nicht raten darf (CLAUDE.md). Hier stehen die Fragen an einer Stelle. Der Mensch trägt die Antworten unten ein (oder direkt in die Tickets); danach setzt eine Sitzung die Tickets auf `todo` und arbeitet sie ab. Eintragen und Tickets freigeben ist hier die ganze Arbeit, kein Code.

## Fertig, wenn …
- Jede Frage unten hat eine Antwort mit Quelle (Seite im Manual, Messung, Entscheidung mit Datum)
- Die Antworten stehen im jeweiligen Ticket (Übergabe), das Ticket ist von `blocked` auf `todo` gesetzt (`python3 tools/board.py status <ID> todo`)
- `python3 tools/board.py validate` grün

## Kontext
tasks/M-08.md, tasks/C-06.md, tasks/SIM-33.md, ADR-022, ADR-027.

## Fragen
**M-08 (RuleGuard, Endgame-Höhe)**
1. Soll es überhaupt eine harte Endgame-Höhe im Midfield geben? Manual v2.1 SG12 nennt keine Zahl (weiche Grenze, mündliche Verwarnung); die Zahl in plan.md ist eine Team-Entscheidung.
2. Wenn ja: welcher Wert in Zoll, welche Quelle?
3. Ist das Midfield ein Rechteck um die Midfield-Posen oder eine Zone in der Feldmitte (Maße)?

**C-06 (Ebenenstufe der Goals)**
4. Wie groß ist die Höhe pro Ebene (`ebenen.stufe_in` in field/field.json) in Zoll: aus CAD 276-9250-000 oder am echten Objekt gemessen?
5. Hängt die Höhe davon ab, ob unten ein Cup oder ein Pin liegt? (wenn ja, braucht `GoalBook::place_height_in` den Objekttyp)
6. Wie kommen die Goal-Daten auf den Roboter (Erzeugung aus field.json zur Build-Zeit o. Ä.)?

**SIM-33 (Maße aus dem offiziellen Manual v2.1)**
7. Welcher Perimeter gilt am Wettkampf und am Testfeld: Portable (140,4″, Manual S. A13) oder Metal (140,5″, S. A14)? (SIM-33-Notiz in der Merkliste: vor dem Event nachfragen)
8. Dateipfad des offiziellen Manual-PDF (liegt bisher nur im Chat als WhatsApp-Export); es soll nach docs/ref/ (Ersatz der inoffiziellen Kopie, siehe C-08)

## Antworten
(Menschen tragen sie hier ein: Nummer, Antwort, Quelle, Datum)

## Hardware-Prüfung
—

## Übergabe
Angelegt aus der Sitzung vom 2026-11-12, bewusst `blocked`: kein KI-Ticket, solange die Antworten fehlen.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-12 · arjanssen (KI) · Ticket angelegt: Fragen aus M-08, C-06, SIM-33 gesammelt · –
