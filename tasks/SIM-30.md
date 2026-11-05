---
id: SIM-30
phase: "5"
titel: Feldobjekte (Cups/Pins, Center-Stacks) sind keine Kollisionsobjekte – nur Roboter, Feldrand und Goals
status: review
owner: DAVIDCOPCOP
branch: ticket/sim-30
depends: []
hw: nein
---

## Ziel
Cups (Cones) und Pins, insbesondere die Center-Stacks in der Feldmitte (CENTER_STACK_R1/R2/B1/B2 in field/field.json, objekte_start), dürfen in der Simulation nie als Hindernis wirken: Kein COLLISION-Ereignis, kein Wegschieben, kein BLOCKED, weder in der C++-Simulation (frostbyte_sim) noch im Browser-Livesim. Heute kollidieren in der Simulation nur Roboter untereinander, der Feldrand und (nur im Browser) die Goals mit 4 in (sim_params live.kollision.radius_in.goals); Objekte sind nicht dabei. Dieses Ticket sichert das ab und schließt Lücken.

Schritte:
1. Prüfen (C++ und tools/viewer/livesim.js): Kann ein Objekt aus objekte_start (Cup/Pin, auch Center-Stack) irgendwo als Kreis/Box in die Kollision geraten, z. B. über live.kollision.radius_in mit einem Abschnitt wie „objekte_start“ oder durch spätere Erweiterung? Ergebnis im Ticket festhalten.
2. Eine feste Regel: Kollisionsabschnitte sind eine Positivliste (aktuell nur goals, später ggf. toggles/loader nach Messung). Objekt-Abschnitte werden beim Laden von sim_params abgelehnt oder ignoriert, mit Hinweis.
3. Tests (C++ und JS, test_livesim.py / livesim_test.js): Roboter fährt durch die Feldmitte über CENTER_STACK_*: keine COLLISION-/BLOCKED-Ereignisse, Pose ungestört. Gegenprobe: Goal-Kollision im Browser bleibt wie bisher.
4. Als ADR-Nachtrag festhalten (Entscheidung des Menschen 2026-11-05): Objekte sind für die Kollision unsichtbar; Aufnahme und Setzen laufen über Ereignisse (SIM-04), nicht über Geometrie.

## Fertig, wenn …
- Tests in C++ und Browser belegen: Fahrt über Center-Stacks und andere Objekte erzeugt keine Kollision und verändert die Pose nicht
- Ein Kollisionsabschnitt für Objekte wird abgelehnt oder ignoriert; Goals-Kollision unverändert
- ADR-Nachtrag geschrieben; Laptop-Tests grün, `python tools/board.py validate` grün

## Kontext
sim/collision.*, sim/world.cpp (Kollisionen), tools/viewer/livesim.js (loadElements), sim/sim_params.json (live.kollision), docs/decisions/ADR-025-livesim-browser.md (Nachtrag SIM-15), field/field.json (objekte_start).

## Hardware-Prüfung
—

## Übergabe
ÄNDERUNG NÖTIG (KI-Review PR #73, 2026-11-05): Das Ticket und der PR gehen in die falsche Richtung. Objekte sind nicht "nie Hindernis"; das eigentliche Problem war, dass die Center-Stacks (CENTER_STACK_R1/R2/B1/B2) in der Simulation KEIN Kollisionsobjekt sind, obwohl sie in echt im Weg stehen. Zu tun: Stacks als Kollisionskreise in C++ (sim/world.cpp) und im Browser (livesim.js), Radius nur in sim/sim_params.json (quelle: schaetzung), Tests umdrehen (Fahrt in einen Stack erzeugt COLLISION/BLOCKED, nicht vom Platzhalterwert abhängig), ADR-025-Nachtrag neu schreiben, Ziel/Schritt 4 oben anpassen. Offene Fragen an den Menschen: nur Center-Stacks oder alle Objekte? Blockiert, schiebt oder stößt der Roboter sie um? Details: Kommentar im PR #73.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-05 · Máté (mit Claude) · Ticket angelegt (als SIM-30, da SIM-29 für Lift vergeben) · –
2026-11-05 · arjanssen (KI-Review) · Review korrigiert: Empfehlung ändern, Richtung falsch (Objekte sollen kollidieren) · –
