---
id: SIM-30
phase: "5"
titel: Feldobjekte (Cups/Pins) sind Kollisionskörper – erst umstoßen, dann schieben
status: in_progress
owner: DAVIDCOPCOP
branch: ticket/sim-30
depends: []
hw: nein
---

## Ziel
Cups und Pins am Boden (objekte_start, ebene 0) sind in der Simulation Kollisionskörper (Entscheidung des Menschen 2026-11-06): Berührt ein Roboter ein stehendes Objekt, kippt es (Ereignis `TOPPLED:<id>`, einmal je Objekt); fährt er weiter, wird es geschoben. In der C++-Simulation (frostbyte_sim) und im Browser-Livesim gleich. Auswirkungen auf Aufnahme und Punkte sind NICHT Teil dieses Tickets (siehe SIM-34).

Schritte:
1. Kreismodell mit Zustand steht/liegt, Schiebe-Anteil des Roboters (`roboter_anteil`), Objekte bleiben im Feld, keine Objekt-Objekt-Kollision (sim/field_objects.*, DriveModel::displace).
2. Zahlen nur in sim/sim_params.json, Block `feldobjekte` (Radius je Art, Anteil, mit quelle); ohne Block keine Objekte.
3. Livesim (livesim.js) spiegelt das Modell, `live.objects()`.
4. Tests mit eigenen Fixture-Werten (C++ und JS, gleiche Szene, Endpose aus frostbyte_sim); ADR-025-Nachtrag SIM-30; Token `TOPPLED:<id>` in docs/log_format.md.

## Fertig, wenn …
- Test: Fahrt in einen Cup erzeugt genau ein TOPPLED, danach wird er geschoben und der Roboter gebremst; Gegenprobe ohne Objekte ist unverändert; Berührung ohne Überlappung tut nichts; Objekt an der Wand bleibt im Feld (C++ und Browser)
- Livesim und C++ erreichen in der gemeinsamen Szene dieselbe Endpose (≤ 0,1 in); Goal-Kollision im Browser unverändert
- ADR-Nachtrag geschrieben; Laptop-Tests grün, `python tools/board.py validate` grün

## Kontext
sim/collision.*, sim/world.cpp (Kollisionen), tools/viewer/livesim.js (loadElements), sim/sim_params.json (live.kollision), docs/decisions/ADR-025-livesim-browser.md (Nachtrag SIM-15), field/field.json (objekte_start).

## Hardware-Prüfung
—

## Übergabe
Stand (2026-11-06): umgesetzt und getestet, bereit für Review. C++: sim/field_objects.*, world.cpp (nach Roboter–Roboter, vor Feldrand), DriveModel::displace; Browser: livesim.js (touchObjects); Tests test/test_sim_field_objects.cpp und livesim_test.js Abschnitt 13; ADR-025-Nachtrag SIM-30. Folgeticket SIM-34 (Wirkung auf Aufnahme/Punkte). Alter PR #73 (Richtung „unsichtbar“) wurde auf demselben Branch zurückgebaut und weiterverwendet.
Am echten Roboter zu prüfen: nichts (reine Simulation). Offen: `roboter_anteil` und Radien sind Schätzwerte bzw. Manualwerte (SIM-33 prüft gegen das PDF); Anteil aus echten Schiebeversuchen bestimmen.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-05 · Máté (mit Claude) · Ticket angelegt (als SIM-30, da SIM-29 für Lift vergeben) · –
2026-11-05 · arjanssen (KI-Review) · Review korrigiert: Empfehlung ändern, Richtung falsch (Objekte sollen kollidieren) · –
2026-11-05 · DAVIDCOPCOP-Sitzung (KI) · blocked: Review verlangt Umbau (Center-Stacks als Kollisionsobjekte); offene Fragen für einen Menschen: nur Center-Stacks oder alle Objekte? blockiert, schiebt oder kippt der Roboter sie? · –
2026-11-06 · matelechner01 (Claude) · Entscheidung des Menschen eingetragen: alle Objekte, erst umstoßen, dann schieben · –
2026-11-06 · DAVIDCOPCOP (KI) · umgesetzt: Objekte kippen und werden geschoben (C++ und Livesim), ADR-Nachtrag, SIM-34 angelegt · –
