---
id: SIM-42
phase: "5"
titel: Livesim nach Handtest überarbeiten: Lift im Boden, Zusammenführen/Stapeln, Physik wirkt unnatürlich
status: in_progress
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
Der Mensch ergänzt/bestätigt die Punkte; dann je Punkt: Test oder Sichtprüfung im Browser (Stacker greift Pin und Cup, hebt auf jede Stufe, setzt auf Goal und Stapel; Lift-Modell folgt `lift.z` sichtbar von unten bis oben und taucht nie unter den Boden); Laptop-Tests grün, `board.py validate` grün.

## Kontext
tools/viewer/livesim.js (doLift, handPos), tools/viewer/index.html (updateLive, info.set), sim/sim_params.json (physik.greifen), ADR-051, SIM-41, M-11.

## Hardware-Prüfung
—

## Übergabe
Stand (2026-11-12): Punkt 1 (Lift) erledigt: `physik.greifen.hand_hoehe` (basis + hub·sin θ aus dem CAD, je Roboter), `liveSim.liftDeg()/handZ()`, Viewer stellt `info.set` im Livesim aus dem Lift; Stufen = Hubbereich/5. Im Browser gesehen (Stacker, Stufe 0 → 5: Lift fährt von unten bis oben, vorher blieb das Modell stehen). Tests 91 + 80 ok, ctest grün.
Zusätzlich erledigt (aus SIM-41, Punkt 2): Greifen nur auf Höhe des Teils (`greif_toleranz_in`, Hand zwischen Fußpunkt und Oberkante), SG6 filtert Kandidaten vor der Auswahl. Tests 93 + 80 ok; Browser: Pin vom Stapel greifen, zum Goal fahren, ablegen → STACK ok.
Rest: ADR-051-Nachtrag, restliche SIM-41-Funde, Physik (Punkt 3) im Browser beurteilen. (alt:) Punkt 2 (Zusammenführen/Stapeln) im Browser durchspielen (Pin + Cup greifen, auf Goal setzen) und die passenden SIM-41-Funde mit rotem Test beheben; danach Punkt 3 (Physik, Auffälligkeiten im Browser suchen).
Prüfstand: Chrome per DevTools-Protokoll (cdp.py in früherer Sitzung), Server `python3 -m http.server 8765` im Repo.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-12 · arjanssen (KI) · Ticket angelegt aus Handtest-Rückmeldung des Menschen · –
2026-11-12 · arjanssen (KI) · Lift im Livesim: Winkel aus Handhöhe, Modell folgt · siehe Branch
2026-11-12 · arjanssen (KI) · Greifhöhe zweiseitig, SG6 vor Auswahl · siehe Branch
