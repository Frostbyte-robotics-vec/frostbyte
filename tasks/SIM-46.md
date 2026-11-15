---
id: SIM-46
phase: "5"
titel: Skills-Routen: Anfahrposen statt Wandpunkte (Runner erreicht den Toggle nicht mit ehrlicher Pose)
status: review
owner: arjanssen
branch: ticket/sim-46
depends: [SIM-44]
hw: nein
---

## Ziel
Befund aus SIM-44 (ADR-016-Nachtrag): Mit GPS-Fusion (ehrliche Pose) sinkt der Punktestand der Skills-Routen von 46 auf 16 (Median, 20 Läufe), auch mit `--noise none`. Ursache: `goto TOGGLE_RED_1` zielt auf einen Punkt an der Wand (x = −71,5); der Runner bleibt bei y ≈ −7,7 hängen, wahre Distanz zum Toggle 12,6 in > `toggle_reichweite_in` (12, Schätzwert). Der Toggle schlägt fehl, rote Neutral-Goals zählen ohne roten Toggle nicht (`sim/scorer.cpp:18`). Die frühere Zahl 46 war ein Zufallstreffer der Odometrie. `target_p10 40` in `routes/skills_stacker.route` ist damit nicht mehr erfüllt (P10 16): keine Regression, sondern die ehrliche Ausgangslage.

Zu tun: Anfahrposen (Punkt vor der Wand, Ausrichtung zum Toggle bzw. Goal, `anfahrt_deg` in field.json) in den Routen benutzen, den Runner-Toggle zuverlässig auslösen, Punkte wieder messen. Zusätzlich prüfen: `goto` bleibt bei 0,5–1 in Seitenversatz stehen, weil ein Differentialantrieb quer nicht korrigiert (Timeout statt Ankunft, z. B. Stacker bei NEUTRAL_R1, ca. 0,9 in); Regler/Toleranz (D-12) oder Route.

## Fertig, wenn …
- Runner-Toggle in 20 Läufen (Seed 1–20) in mindestens 90 % der Läufe ausgelöst (Messwert vorher/nachher)
- Punkte-Median der Skills-Routen vorher/nachher als Zahlen im Ticket; `target_p10` neu festgelegt oder begründet
- Tests von Platzhaltern entkoppelt (`toggle_reichweite_in` ist `schaetzung`: Test mit eigenem Fixture-Wert), Laptop-Tests, beide Roboter, `board.py validate` grün

## Kontext
tasks/SIM-44.md, docs/decisions/ADR-016-gps-fusion.md (Nachtrag), ADR-031/034, routes/skills_*.route, field/field.json (`anfahrt_deg`), SIM-27, A-05, D-12.

## Hardware-Prüfung
—

## Übergabe
(aktueller Stand für die nächste Sitzung – wird bei jedem Sitzungsende überschrieben)

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-15 · arjanssen (KI) · Ticket angelegt (Befund aus SIM-44) · –
