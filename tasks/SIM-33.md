---
id: SIM-33
phase: "5"
titel: Maße aus dem offiziellen Game Manual v2.1 übernehmen (Spielfeld innen 140.4″ statt 144″, Objektmaße)
status: todo
owner:
branch:
depends: []
hw: nein
---

## Ziel
Der Mensch hat das saubere offizielle Game Manual v2.1 (PDF, 148 Seiten, Appendix A mit Maßzeichnungen) geliefert. Daraus die Zahlen in field/field.json und sim/sim_params.json prüfen und übernehmen. Quellen mit Seite angeben (`quelle: "Game Manual v2.1, Appendix A, S. A…"`).

Gefundene Abweichung (zuerst prüfen): `feld_in` ist 144, das ist das Kachelraster (12′ × 12′ Matte). Befahrbar ist aber nur das Innenmaß zwischen den Wänden: **140.4″** (Portable Field Perimeter, S. A13) bzw. **140.5″** (Metal Field Perimeter, S. A14); Wandhöhe 11.54″/11.5″. Die Goal-Positionen im Manual (A10, Ursprung Ecke, Feldmitte bei 70.20″) passen zu field.json (Mitte → x = −47.091 entspricht 23.109 ✓), nur die Wand sitzt 1.8″ näher an der Mitte als in der Simulation (70.2 statt 72). Betrifft Wandkollision (sim/collision.cpp, world.cpp), Livesim und Start-Posen an der Wand (A-08).

Weitere Maße aus dem Manual (alle Zoll), zum Abgleich und als Messwerte für SIM-30 und C-06:
- Pin: 6.50 hoch, größter Ø 3.16 (Flansch), Kegel unten Ø 1.40 / oben Ø 2.35, Pin-Hälfte 2.93 (A5)
- Cup: 6.48 hoch, Ø 3.16 oben, Ø 2.32 an der Naht (A6)
- Goal: Grundfläche 5.61 (quadratisch-achteckig, Eckradius 3.22), Höhe Allianz-Goal 3.25, Quadrant-Goal 5.77, Mittel-Goal 8.77, Öffnung Ø 2.37 (A7)
- Toggle: 25.99 lang, Dreieck 2.03 hoch, Montagehöhe 13.20 über Boden (A8)
- Loader: 4.02 breit, 14.37 (unten) bis 22.97 (oben) hoch (A9)
- Tolleranzen: Feldelemente ±1.0″, Startpositionen der Objekte ±1″ (T5)
- Autonomous Line / Midfield: Linie 2.5 breit, Midfield-Quadrat Diagonale 34.05 (A11)
- Startgrößen/Regeln: SG1 18″, VEX U 24″ und 15″ (VUR1), SG2 24″ × 24″, SG3 50″ (bereits im RuleGuard)

Mit offen: Stufe im Stapel (C-06) steht NICHT im Manual als Zahl; nur Bilder (SC2-1). Weiter messen.

## Fertig, wenn …
- Entscheidung dokumentiert (ADR-Nachtrag): feld_in = Innenmaß 140.4 (Portable) oder 140.5 (Metal)? Welcher Perimeter am Wettkampf/Testfeld? (Frage an den Menschen)
- field.json und sim_params.json mit Quelle und Seite aktualisiert; Tests, die vom alten Wert abhängen, mit eigenen Fixture-Werten statt Platzhaltern
- docs/ref/game_manual_v2.1.pdf/.txt durch das offizielle PDF ersetzt (siehe C-08), README dort angepasst
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün

## Kontext
tasks/C-08.md, tasks/C-06.md, tasks/SIM-30.md, docs/ref/README.md, field/field.json (`feld_in`), sim/collision.cpp.

## Übergabe
Noch nicht begonnen. Die PDF-Datei liegt aktuell nur im Chat (WhatsApp-Export, DOC-20261019-WA0024.pdf); für das Ersetzen in docs/ref/ braucht die Sitzung den Dateipfad vom Menschen.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-06 · matelechner01 (Claude) · Ticket angelegt: Maße aus dem offiziellen Manual, Abweichung feld_in 144 vs. 140.4 gefunden · –
