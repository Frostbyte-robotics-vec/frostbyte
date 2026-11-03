# Wertung im Skills-Match (RSC3) – Regeln und Handrechnungen

Quelle: Game Manual 2026–2027 v2.1, Regeln RSC3 (PDF-Seite 32), SC2–SC5 (S. 17–19), SC6 (S. 19), VURS1–VURS3 (VEX U). Eigene Worte, Regelnummern in Klammern.
Der Scorer `sim/scorer.*` (SIM-10, ADR-024) setzt genau das um.

**Achtung:** Die Safari-Kopie des Manuals (C-07) ist am rechten Seitenrand abgeschnitten. Betroffen sind u. a. RSC3a (Satzende), RSC3a.iii.1, der Hinweis zu SC2 (nested), SC5b und die Cup-Definition. Dieser Auszug stützt sich auf den lesbaren Teil; er muss gegen das vollständige Manual geprüft werden.

## Regeln
- **Placed (SC2):** Der unterste Gegenstand eines Stapels muss ein Pin sein (in das Goal eingetaucht). Darüber muss sich Cup und Pin
  abwechseln (Pin, Cup, Pin, Cup …). Ein Cup gilt nur auf einem Placed Pin als Placed, ein Pin nur im Goal oder in einem Placed Cup.
  Ab der ersten Stelle, die das verletzt (zwei Pins hintereinander, Cup als Erstes, zwei Cups hintereinander), zählt nichts mehr darüber.
- **Sichtbar (SC3):** Eine Pin-Hälfte ist sichtbar, außer sie steckt im Goal oder in der undurchsichtigen Cup-Hälfte.
  Untere Hälfte des untersten Pins: im Goal, unsichtbar. Obere Hälfte eines Pins unter einem Cup: steckt in der unteren Cup-Hälfte.
  Untere Hälfte eines Pins über einem Cup: steckt in der oberen Cup-Hälfte. Obere Hälfte des obersten Pins: sichtbar.
- **Welche Cup-Hälfte undurchsichtig ist,** steht in unserer (abgeschnittenen) Kopie nicht lesbar (Cup-Definition S. 74); im vollständigen Manual prüfen.
  Der Scorer nimmt es deshalb als Eingabe `opaque_lower` (true = untere Hälfte undurchsichtig).
- **Punkte je sichtbarer Hälfte (RSC3a):**
  - rot: 5 Punkte, nur in einem roten Quadranten oder im Midfield-Goal
  - blau: 5 Punkte, nur in einem blauen Quadranten oder im Midfield-Goal
  - gelb: 10 Punkte, nur wenn Owned (SC5, RSC3a.iii):
    - Quadranten-Goal: Toggle des Quadranten steht auf der Farbe des Quadranten (rot im roten, blau im blauen). Gelb oder nicht gesetzt: 0.
    - Midfield-Goal: mindestens ein Roboter endet im Midfield (anders als im Head-to-Head: dort zählt die Mehrheit, SC5b).
  - Hälften, die nach RSC3a.i/ii nicht zählen, geben 0 (kein Abzug).
- **Roboter im Midfield (RSC3b, SC6):** 8 Punkte je Roboter, der bei Matchende zumindest teilweise im Midfield steht. In VEX U sind es bis zu zwei Roboter (VURS2), also höchstens 16.
- VURS1 ändert nur die Match Loads (4 rot/gelb, 6 blau/gelb, 10 Cups), nicht die Wertung.

## Schreibweise in den Handrechnungen
Pin(unten, oben) mit den Farben der beiden Hälften. Cup(undurchsichtig unten) oder Cup(undurchsichtig oben). Stapel von unten nach oben.

## Handrechnungen (von der KI aus dem Regeltext gerechnet, Bestätigung durch einen Menschen offen)
1. **Leeres Feld, 1 Roboter im Midfield.** 8 (RSC3b).
2. **Rotes Quadranten-Goal, Toggle rot, [Pin(rot, gelb)].** Untere Hälfte im Goal unsichtbar. Obere gelbe Hälfte sichtbar, Toggle rot = Quadrant rot → Owned: 10.
   Gleicher Stapel mit Toggle gelb/neutral oder blau: 0 (RSC3a.iii.1).
3. **Blaues Quadranten-Goal, Toggle blau, [Pin(rot, blau), Cup(untere Hälfte undurchsichtig = nein), Pin(gelb, rot)].**
   Pin 1 unten im Goal unsichtbar; Pin 1 oben (blau) steckt in der durchsichtigen unteren Cup-Hälfte → sichtbar, blau im blauen Quadranten: 5.
   Pin 2 unten (gelb) steckt in der oberen, undurchsichtigen Cup-Hälfte → unsichtbar. Pin 2 oben (rot) sichtbar, aber rot im blauen Quadranten: 0.
   Summe 5.
4. **Midfield-Goal, 2 Roboter im Midfield, [Pin(gelb, rot), Cup(untere Hälfte undurchsichtig = ja), Pin(blau, gelb)].**
   Pin 1 unten im Goal unsichtbar. Pin 1 oben (rot) steckt in der undurchsichtigen unteren Cup-Hälfte → unsichtbar.
   Pin 2 unten (blau) steckt in der durchsichtigen oberen Hälfte → sichtbar, blau im Midfield: 5.
   Pin 2 oben (gelb) sichtbar, Owned (≥ 1 Roboter im Midfield): 10. Goal 15, Roboter 2 · 8 = 16 → **31**.
5. **Zwei Pins direkt übereinander (Regel SC2a.ii), rotes Quadranten-Goal, Toggle rot, [Pin(blau, gelb), Pin(rot, rot)].**
   Pin 2 ist nicht Placed (kein Cup dazwischen), zählt nicht. Pin 1 oben gelb sichtbar (kein Cup darüber), Owned: **10**.
6. **Mehrere Goals addieren.** Fall 2 (10) + Fall 4 (31) im selben Spiel, die zwei Roboter nur einmal gezählt → 10 + 15 + 16 = **41**.
7. **Cup als Erstes im Goal, [Cup, Pin(rot, rot)]:** nichts ist Placed → 0.
