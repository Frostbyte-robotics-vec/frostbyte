# ADR-024: Scorer nach RSC3 (sim/scorer.*)

Status: angenommen (3. Nov 2026), Ticket SIM-10. Der Advisor (Opus) hat vorgegeben: VEX-U-Regeln (VURS) vorher lesen, Handrechnungen vor dem Code mit Regelnummern schreiben, Placed und Sichtbarkeit im Scorer berechnen (nicht vorberechnet übergeben), Cup-Ausrichtung als ausdrückliche Eingabe.

## Entscheidung
- `sim/scorer.hpp/.cpp`: reine Funktion `score_skills(ScoreInput)` → `ScoreResult` (Aufschlüsselung rot/blau, gelb, Midfield-Roboter, Summe). Keine Abhängigkeit von field.json; Tests hängen nicht an Platzhaltern.
- Eingabe je Goal: Zone (roter/blauer Quadrant, Midfield), Toggle des Quadranten (nicht gesetzt = neutral, SC4) und der Stapel von unten nach oben (Pin mit Farbe unterer/oberer Hälfte, Cup mit `opaque_lower`).
- Der Scorer entscheidet selbst, was Placed (SC2: Pin, Cup, Pin, … ab dem ersten Verstoß zählt nichts mehr) und sichtbar (SC3) ist.
- Gelb im Midfield: Owned, wenn mindestens ein Roboter im Midfield endet (RSC3a.iii.2, anders als die Mehrheitsregel im Head-to-Head SC5b). Roboter im Midfield: 8 Punkte, begrenzt auf 0..2 (VEX U, VURS2).
- Punktwerte sind Regelkonstanten aus dem Manual (5/10/8), keine Modellzahlen; sie stehen daher im Header, nicht in sim_params.json.

## Grenzen / offen
- Welche Cup-Hälfte undurchsichtig ist, steht im Manual nicht lesbar (Cup-Definition S. 74 im PDF abgeschnitten). Deshalb Eingabe `opaque_lower`; wer den Scorer füttert (Objektmodell, SIM-04), muss das festlegen, bis es am echten Cup geprüft ist.
- Modell kennt nur Stapel aus Pins und Cups im Goal; keine Höhenprüfung (zu hohe Stapel, Toggle-Berührung durch Roboter SC4b) und keine Verletzungen.
- Der Scorer ist noch nicht in `run_match` eingehängt (eigenes Ticket, wie bei Kollisionen/Regelprüfer).
- Die Handrechnungen (docs/rules_scoring.md) sind von der KI aus dem Regeltext gerechnet; Bestätigung durch einen Menschen steht aus.
