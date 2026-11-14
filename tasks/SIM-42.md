---
id: SIM-42
phase: "5"
titel: Livesim nach Handtest überarbeiten: Lift im Boden, Zusammenführen/Stapeln, Physik wirkt unnatürlich
status: done
owner: arjanssen
branch: ticket/sim-42
depends: [SIM-40]
hw: nein
---

## Ziel
Handtest des Menschen am 2026-11-12 (Livesim im Browser): „funktioniert noch richtig schlecht“. Gemeldet (Wortlaut sinngemäß, Nachricht brach ab, ggf. ergänzen):
1. Der Lift fährt in den Boden hinein und nicht ganz nach oben.
2. Teile „zusammenmachen“ (Pin + Cup zusammenführen/stapeln) klappt nicht gut.
3. Die Physik sieht noch nicht gut aus.

Vermutungen (nicht geprüft, zuerst mit Test oder Blick im Browser bestätigen):
- zu 1: `livesim.js` führt nur `r.lift.z` (Hand-Höhe, max. `lift_stufen` × `lift_stufe_in` = 5 × 4,5 = 22,5 in). Das CAD-Modell im Viewer wird aber aus `lift_deg` des Logs gestellt (index.html, `info.set`); im Livesim wird `lift_deg` nicht aus `lift.z` abgeleitet, also bleibt das Modell auf `thMin` (unten) oder verläuft falsch. Außerdem erreicht 22,5 in nie die 50″ (SG3). Hub und Stufen sind Schätzwerte; richtig wäre Hub aus dem CAD (M-11).
- zu 2: SIM-41 enthält schon Befunde dazu (Greifen mit angehobener Hand, `settle()`, `release()`, Fangradius); mit SIM-41 abstimmen, nicht doppelt arbeiten.
- zu 3: Rückprall, Reibung, Kippen: Werte sind Schätzungen (`physik`); konkret beschreiben lassen, was falsch aussieht.

## Fertig, wenn …
- Lift-Modell folgt dem Lift im Livesim sichtbar von unten bis oben, steht nie im Boden (Browser gemessen: minY ≈ 0 auf jeder Stufe; Klaue = Handhöhe)
- Eine Stufe = eine Stapelebene (`lift_stufe_in`), oberste Stufe endet bei `max_deg`
- Greifen nur auf Teilhöhe, SG6 vor der Auswahl, Pin und Cup in der Hand ineinander, Q–F–Q stapelt (Browser)
- Tests mit Fixture-Werten (95 + 80), Laptop-Tests und `board.py validate` grün, ADR-051-Nachtrag
- Physik (Punkt 3) **nur teilweise**: `release()` setzt Neigung zurück (Test). Der Rest (Kippen nach Gefühl, `toLying` ohne Überlappungsauflösung, `settle`, `pushStanding`) steht in SIM-43; ob das Aussehen besser ist, kann nur der Mensch beurteilen
- Offene SIM-41-Funde: bleiben in SIM-41 (hier erledigt: Modellzahlen ±0,3, Greifhöhe zweiseitig, SG6 vor der Auswahl)

## Kontext
tools/viewer/livesim.js (doLift, handPos), tools/viewer/index.html (updateLive, info.set), sim/sim_params.json (physik.greifen), ADR-051, SIM-41, M-11.

## Hardware-Prüfung
—

## Übergabe
Stand (2026-11-12): fertig bis auf Physik (siehe SIM-43), PR offen. Neu seit dem ersten Stand: Greifhöhe (`greif_hoehe_in`), Klaue an der Haltestelle (LIVE_CLAW_T, basis_in 21,34), Pin sinkt nach dem Cup nicht ein, release setzt theta zurück. Lift (hand_hoehe, liftDeg/handZ, Viewer stellt das Modell), Stufen = lift_stufe_in, Greifhöhe, SG6 vor der Auswahl, Pin auf Cup in der Hand, Q zuerst Cup. Mutationstest: ohne Höhenprüfung bzw. ohne SG6-Filter werden die jeweiligen Tests rot. SIM-40-Abhängigkeit: PR #116 ist gemergt, das Board war nur nicht nachgezogen.
Am Rechner zu prüfen: Fühlt sich der Lift richtig an (Stufenhöhe 4,5 in, Tempo 20 in/s: Schätzwerte)? Stimmen `basis_in`/`hub_in` mit dem echten Roboter (M-11)? Der Greifarm bleibt im Livesim in Greifstellung (Klaue hängt am Schlitten), gehaltene Teile erscheinen 2,5 in vor dem Umriss: stimmt das optisch? Physik: siehe SIM-43.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-12 · arjanssen (KI) · Ticket angelegt aus Handtest-Rückmeldung des Menschen · –
2026-11-12 · arjanssen (KI) · Lift im Livesim: Winkel aus Handhöhe, Modell folgt · siehe Branch
2026-11-12 · arjanssen (KI) · Greifhöhe zweiseitig, SG6 vor Auswahl · siehe Branch
2026-11-12 · arjanssen (KI) · Stufen = Stapelebene, Pin auf Cup in der Hand, ADR-Nachtrag, Punkt 3 nach SIM-43 abgespalten · siehe Branch
2026-11-12 · arjanssen (KI) · Griffhöhe, Klaue/Haltestelle gemessen, release-Fix, ADR · siehe Branch
