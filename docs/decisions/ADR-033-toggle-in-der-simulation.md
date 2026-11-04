# ADR-033: Befehl toggle und Toggle-Anfangszustand in der Simulation

Status: angenommen (4. Nov 2026), Ticket SIM-23. Ändert die Annahme aus ADR-032 („Toggles stehen für Routen in Zielfarbe“).

## Entscheidung
- `Game::start_toggle(t_ms, robot, toggle)` setzt nach `toggle_ms` den Toggle auf seine Zielfarbe (`field.json` `toggles[].zielfarbe`) und damit die Toggle-Farbe aller Goals seines Quadranten (`quadrant`). Antwort wie bei Zyklen (`CycleTicket`); der Roboter ist `toggle_ms` beschäftigt. Unbekannter Toggle oder beschäftigter Roboter: abgelehnt (`rejected_cycles`). `cancel` (Timeout des Players) zieht einen laufenden Toggle zurück, nach 0:00 zählt keiner mehr. Das Setzen gelingt immer (kein Zufall, kein Modell dafür).
- `SimRouteExecutor` ruft `start_toggle` für `toggle`; ohne Spielzustand bleibt es ein Platzhalter (ADR-031).
- `sim_params.json` `spiel.toggle_dauer_s` (Pflicht, > 0, Quelle `schaetzung`, nie gemessen) ist die Dauer. Sie gilt für jeden Toggle gleich.
- **Anfangszustand:** `spiel.toggle_anfang` ist jetzt `neutral`. Quelle: Game Manual v2.1, Appendix A, PDF-Seite 71 „Toggle Assembly – Starting Orientation“ („Yellow facing up and into the field“, Allianzfarbe nach außen) und SC4 (nicht gesetzt = neutral/gelb). Die Option `zielfarbe` bleibt für Tests.
- Nicht modelliert: SC4b (ein Roboter berührt den Toggle bei Matchende, dann zählt er als neutral), Fehlschläge beim Drehen, ein Gegner, der den Toggle zurückdreht (Skills hat keinen).

## Folge
- Bisher waren die Toggle-Punkte gratis (Start in Zielfarbe, `toggle` ohne Dauer). Ergebnisse vor SIM-23 sind in dieser Hinsicht zu optimistisch, ein Modell kann P10 nur senken.
- Messung (A-05, Routen v1, Posen aus A-13, 20 Läufe, alles Schätzwerte der Sim): P10 90,5 mit den Route-Timeouts 4,0 s bei `toggle`, sowohl mit `neutral` als auch mit `zielfarbe`. Mit den Timeouts der v1-Route (2,0/3,0 s, kürzer als `toggle_dauer_s` = 2,5 s) bricht der Player die ersten Toggles ab, nichts wird gesetzt, und P10 fällt auf 56. Der Einbruch kommt vom Timeout, nicht vom Startzustand. Dass die Toggle-Zeit P10 gar nicht senkt, ist nicht weiter untersucht.
- Route-Timeouts von `toggle` müssen also mindestens `toggle_dauer_s` betragen (A-05), und `toggle_dauer_s` ist zuerst am Roboter zu messen.

## Ergänzung SIM-25: Reichweite
- `Game::start_toggle(t_ms, robot, toggle, x_in, y_in)` lehnt ab (`rejected_cycles`, Schritt `failed`), wenn der Abstand der Roboterpose zum Toggle (`field.json` `x_in`, `y_in`) größer als `GameSetup::toggle_reach_in` ist. Ein abgelehnter Toggle macht den Roboter nicht beschäftigt und setzt nichts.
- Die Pose ist die wahre Pose des Roboters (`SimRouteExecutor::set_pose`, vom `RouteRunner` je Steuertakt aus `drive->pose()` gesetzt), nicht die Schätzung: Ob der Arm den Toggle erreicht, hängt von der wahren Lage ab. Die Schätzung dient nur zum Hinfahren. (Die Odometrie-Fehler der Simulation liegen bei 12 bis 70 in, größer als die Reichweite.)
- `sim_params.json` `spiel.toggle_reichweite_in` (Pflicht, > 0, `quelle` schaetzung, 12 in, nie gemessen; Messung: Arm am Runner, M-11/TGL). Der Abstand ist Mitte Roboter bis Toggle-Position.
- **Folge:** Die Routen v1 setzen die Toggles von der Stelle aus, an der der Runner steht (kein `goto` vor den Toggles, Toggles liegen an den Wänden bei ±71,5 in). Mit 12 in Reichweite wird jeder `toggle` sofort abgelehnt (Schritt `failed`, 0 s), alle Goals bleiben neutral, gelbe Pins zählen nicht (SC4), und die Simulation von `routes/skills_runner.route` fällt von 56 auf 0 Punkte (20 Läufe, Schätzwerte). Gegenprobe: dieselbe Route ohne die `toggle`-Zeilen (ohne Reichweiten-Logik) liefert ebenfalls 0 Punkte. Die Route muss zu den Toggles fahren (Ticket A-05, dort vermerkt). Die 0 sagt nichts über die Route nach A-05 aus und hängt am geschätzten Wert.
- **Nebenbefund (nicht Teil von SIM-25, Ticket SIM-26):** Weil die abgelehnten Toggles keine Zeit mehr kosten, startet das erste `goto` bei 0,3 s statt bei 9,8 s. Dann ist der Odometriefehler des Runners 72 in statt 12 in (Median `odo_err_max_in`); die Schätzung hinkt der wahren Pose in der ersten Fahrt um bis zu 70 in hinterher, die Route-Timeouts laufen ab. Dasselbe zeigt die Route ohne `toggle`-Zeilen, es liegt also nicht an der Reichweite, sondern am Modell der Odometrie beim frühen Start.
