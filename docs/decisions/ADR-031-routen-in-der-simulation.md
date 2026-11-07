# ADR-031: Routen in der Simulation

Status: angenommen (3. Nov 2026), Ticket A-04. Der Advisor (Opus) hat vorgegeben: A-04 ist nicht A-10 (kein Roboter-Adapter) und kein Spielzustand aus Routen; Sim-Executor in `sim/`, eine Bewegung zur Zeit, Signalbus über beide Roboter, `toggle` am Stacker beim Laden ablehnen, Gegenprobe ohne Kollision, Tests unabhängig von Platzhaltern.

## Entscheidung
- `load_config` parst beide Routen mit `parse_route` (ADR-023) und `route_names_from_field`; Fehler werden zu `std::runtime_error("<Datei>: Zeile N: …")`. Ein `toggle` in einer anderen als der Runner-Route wird beim Laden abgelehnt (der Parser kennt den Roboter nicht). Der Namenstabellen-Zugriff passiert erst, wenn eine Route Zeilen hat: Testfelder ohne `goals`/`loader`/… bleiben mit leerer Route ladbar.
- `sim/route_runner.hpp/.cpp`: `RouteRunner` (je Roboter ein `RoutePlayer`, ein `MotionController` mit eigenen `Pid`, ein `SimRouteExecutor`) mit stabilen Adressen; `run_match` baut ihn für jeden Roboter mit Startpose, Route und ohne eigene `closed_loop`. Pro Steuertakt: `player.update(t, t)` (Matchzeit = Simulationszeit), dann `motion.update` mit der **geschätzten** Pose. Handsteuerung und `closed_loop` haben weiter Vorrang.
- `SimRouteExecutor`: `goto <NAME>` löst Koordinaten aus `field.json` auf (`FieldPoint`); ein Punkt mit `null`-Koordinate scheitert sofort (`failed`), es wird nie nach (0, 0) gefahren. `goto pose` und `turn` gehen an den MotionController. Eine Bewegung zur Zeit: startet eine zweite Lane `goto`/`turn`, scheitert die erste. `cancel` meldet `done` nur, wenn das Ziel erreicht war, und stellt den Antrieb auf 0 V.
- `signal`/`wait_for` laufen über einen `SignalBus`, den beide Roboter eines Matches teilen (deterministisch; der Stacker wird im Takt zuerst gerechnet: ein Signal des Runners kommt beim Stacker einen Takt später an, eines des Stackers beim Runner im selben Takt).
- `intake`, `cycle`, `toggle`, `start` sind **Platzhalter ohne Modell**: sie enden sofort mit `done`. Es gibt dafür keine Zahl in `sim_params.json`, weil nichts modelliert wird. Der Anschluss an `Game`/`ObjectModel` (Zyklus-Dauer, Besitz, Punkte aus der Route) ist mit SIM-22 erledigt (ADR-032); ohne Spielzustand (keine Route mit `cycle`/`intake`) bleiben sie Platzhalter.
- Reglerwerte und Standard-Timeouts je Roboter stehen in `RobotSetup::control` (`RouteControl`). `frostbyte_sim` setzt sie aus `config/stacker.hpp` und `config/runner.hpp` (gleiche Werte wie am Roboter); Tests setzen eigene.
- Die Log-Spalte `route_step` (Format v1, schon vorhanden) zeigt `Zeile:Befehl Name` der Hauptlane, z. B. `2:goto_pose`.
- Bei `goto pose x= y= h=` wird `h` ignoriert (`move_to` fährt nur x/y), `goto pose` kennt kein `tol=`. `goto <NAME> tol=` wirkt seit D-14 (ADR-017, Nachtrag D-14); den Ausführer am Roboter baut A-10.

## Grenzen
- Mit `field/field.json` laufen die Skills-Routen noch nicht: `START_STACKER`/`START_RUNNER` sind `null` (A-01), also gibt es kein Fahrmodell und keine Route. Dort, wo Posen gesetzt sind, sind `goto`-Ziele (Loader, Goals) Feldpunkte-Mitten, nicht Anfahrposen vor dem Element (A-05).
- (bis SIM-22, jetzt ADR-032) `cycle` ändert den Spielstand nicht; die Spalten `score`, `has_cup`, `has_pin` bleiben wie bisher (nur Demo).
- Kollision der Roboter untereinander und mit dem Rand: unverändert aus SIM-05; die Route reagiert nicht darauf (kein Ausweichen).

## Am Roboter zu prüfen
- Nichts; erst A-10 (Adapter) bindet dieselbe Schnittstelle an echte Subsysteme.
