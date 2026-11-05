---
id: SIM-29
phase: "5"
titel: Lift-Bewegung in der Simulation: Liftwinkel im Match fahren, ins Log schreiben, im Viewer anzeigen
status: done
owner: arjanssen
branch: ticket/sim-29
depends: [SIM-04, SIM-07, SIM-22]
hw: nein
---

## Ziel
Die Lift-Gelenke aus SIM-04 (sim/mechanism.*) laufen bisher nur in Tests und in einzelnen Zyklen, nicht im ganzen Match (ADR-009: „nicht in run_match eingehängt“). Ziel: In einem simulierten Match fahren Stacker und Runner ihren Lift wirklich (Sollhöhe aus Route/Zyklus, mit Geschwindigkeits- und Beschleunigungsgrenze), der Liftwinkel steht im Log, und der Viewer (tools/viewer, Modelle aus robots.js) bewegt den Lift sichtbar mit.

Schritte:
1. Prüfen, was schon da ist: Kommen Lift-Sollwerte aus den Zyklen (SIM-22, ADR-032) schon in run_match an? Was zeichnet robots.js bereits (Gelenke/Namen im Viewer)? Ergebnis kurz im Ticket festhalten.
2. Lift-Gelenk in run_match einhängen (je Roboter, Grenzen aus sim/sim_params.json, nie aus der Platzhalter-Annahme im Code). Gleicher Seed = byte-gleiches Ergebnis.
3. Log-Format: Spalte für Liftwinkel/-höhe ergänzen. Das ist eine Formatänderung → neue Versionsnummer in der Kopfzeile (CLAUDE.md), docs/log_format.md anpassen, Parser (tools/viewer/logparse.js) abwärtskompatibel halten.
4. Viewer: Lift im 3D-Modell aus der Logspalte bewegen (Stacker und Runner), ältere Logs ohne Spalte laufen weiter.
5. Entscheidung (Format, Gelenkgrenzen) als ADR festhalten; bei neuer Logspalte den Advisor fragen (CLAUDE.md).

## Fertig, wenn …
- Ein simulierter Skills-Lauf beider Roboter schreibt einen Liftverlauf ins Log (Stacker und Runner), Lift fährt nie über die Endlagen/SG3-Höhe (RuleGuard bleibt aktiv)
- Im Viewer bewegt sich der Lift beim Abspielen eines neuen Logs sichtbar mit; alte Logs ohne Spalte laden weiterhin
- Test: Liftverlauf ist bei gleichem Seed byte-gleich; Grenzwerte (Endlage, Beschleunigung) sind getestet; Tests hängen nicht von Platzhalterwerten ab
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün

## Kontext
tasks/SIM-04.md, docs/decisions/ADR-009-mechanismen-und-objekte.md, ADR-032-zyklen-aus-routen.md, docs/log_format.md, tools/viewer/robots.js, tools/viewer/logparse.js, tasks/M-01.md (Lift am echten Roboter, needs_hw).

## Hardware-Prüfung
—  (reine Simulation; echte Lift-Grenzen kommen mit M-01/M-11, bis dahin sim_params „schaetzung“)

## Übergabe
Stand: fertig, im PR. Schritt 1 ergab: Logspalten lift_deg/lift_set_deg und die Viewer-Anzeige gab es schon, es fehlte nur das Schreiben in run_match → kein Formatwechsel (siehe ADR-009 Nachtrag). Game hält Lift-Joint je Roboter, Hub/Rückfahrt je Zyklus, cancel setzt auf unten. Test: test/test_sim_game_lift.cpp. Viewer nicht im Browser angesehen (kein Browser in der Sitzung); Logik liest unverändert lift_deg.
KI-Review PR #72 abgearbeitet: main gemergt. Das SG3-Ticket (Folge von SIM-29) heißt jetzt SIM-32, weil SIM-31 auf main schon vergeben war. SG3-Prüfung in der Sim und Sicht im Viewer bewusst abnicken.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-05 · Máté (mit Claude) · Ticket angelegt (als SIM-29, da SIM-28 belegt) · –
2026-11-05 · arjanssen (Claude) · Lift im Match: Game-Joint, Log-Spalten, Tests, ADR-009 Nachtrag · 87cc54c
