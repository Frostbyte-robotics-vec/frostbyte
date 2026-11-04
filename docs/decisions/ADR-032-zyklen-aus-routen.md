# ADR-032: Zyklen aus Routen im Spielzustand der Simulation

Status: angenommen (3. Nov 2026), Ticket SIM-22. Der Advisor (Opus) hat vorgegeben: `start_cycle` aus `Game::step` herauslösen (Demo bleibt byte-gleich), `cancel` für Timeouts, `level=auto` = Teile auf dem Goal, `intake` = `pick()` und `cycle` = der Rest, Spielzustand nur wenn eine Route `cycle`/`intake` hat, Toggle-Anfangszustand als ausdrückliche Annahme.

## Entscheidung
- `Game` bekommt drei Aufrufe, die ein `CycleTicket {accepted, ok, done_ms}` liefern bzw. abbrechen:
  - `start_intake(t_ms, robot, Source)`: `ObjectModel::pick`. Dauer und Fehlschlag aus SIM-04. Der Roboter hält höchstens eine Einheit (Vereinfachung, SG6 erlaubt Pin + Cup; die Route holt je Zyklus eine).
  - `start_cycle(t_ms, robot, goal, level, preload)`: Greifer, Lift, Setzen (`ObjectModel::place_unit`). `run_cycle` ist jetzt `pick` + `place_unit`, gleiche Ziehungen in gleicher Reihenfolge: das Demo bleibt byte-gleich (geprüft mit `frostbyte_sim --demo --seed 1`).
  - `cancel(t_ms, robot)`: zieht laufenden Zyklus und laufende Aufnahme zurück (Timeout des Players). Nichts landet im Stapel, der Roboter ist frei.
- `Game::step` startet die Szenario-Zyklen (`GameSetup::cycles`, Demo) über dieselbe interne Funktion `launch`.
- Ein Befehl wird mit `accepted = false` abgelehnt (gezählt in `rejected_cycles`), wenn der Roboter beschäftigt ist, nichts in der Hand ist oder die Ebene nicht passt. Der Executor meldet dann `failed` (on_timeout greift).
- `SimRouteExecutor` meldet `done`/`failed` erst bei `done_ms` (ganzzahlig in ms verglichen). Bricht der Player ab, ruft `cancel` das `Game::cancel` auf.
- **level**: `auto` = Zahl der Teile auf dem Goal einschließlich der unterwegs (field.json `ebenen`: „Ebene n = n Objekte liegen schon“). Ein ausdrückliches `level=n` muss genau dazu passen, sonst abgelehnt. Ab 6 Teilen auf dem Goal (Ebene > 5): abgelehnt.
- **intake**: Quelle `loader`, wenn das letzte `goto <NAME>` ein Ladepunkt aus `loader` in field.json war, sonst `floor`. Die Einheit wird nach `done_ms` verfügbar.
- **cycle / source**: Hält der Roboter eine aufgenommene Einheit, nimmt der Zyklus keine neue auf (`source=` wird ignoriert; der Parser setzt `preload` als Standard, das darf nicht scheitern). Sonst nur `source=preload` und nur einmal je Roboter (Preload, ohne `pick`, Ausrichtung wie ein Pin vom Loader: Annahme, kein eigener Messwert). `quiver` ohne vorherige Aufnahme ist nicht modelliert und wird abgelehnt.
- **Besitz** (`has_pin`/`has_cup`) gilt wie in ADR-026 ab Start des Zyklus bis zu seinem Ende, nicht schon nach der Aufnahme (Pin oder Cup steht erst fest, wenn das Goal bekannt ist, SC2).
- **Goals** aus field.json: Zone aus `quadrant` (`RED_*`, `BLUE_*`, `MIDFIELD`), Name = `id`. Der Anfangszustand der Toggles steht nicht in field.json und der Scorer macht davon die Punkte der Quadranten-Goals abhängig: ausdrückliche Annahme `spiel.toggle_anfang` in `sim/sim_params.json` (`zielfarbe` = Toggle steht in der Farbe des Quadranten, `neutral`). Der Befehl `toggle` ist weiter Platzhalter. Quelle `schaetzung`, kein Test behandelt den Wert als Wahrheit (die Testfelder haben eigene Werte).
- **Aktivierung**: `load_config` füllt `GameSetup` aus den Routen nur, wenn eine Route `cycle` oder `intake` enthält. Sonst bleiben beide Platzhalter (sofort `done`), `sim_params.json` braucht dann weder `ereignisse` noch `spiel`, und die Spalte `score` bleibt leer wie in ADR-031.
- Zufall: die Seeds des `Game` sind dieselben wie in ADR-026; gleicher Seed = byte-gleiche Logs.

## Grenzen
- Die Roboter fahren weiter nicht „an das Goal“: `cycle` prüft nicht, ob der Roboter am Goal steht (Anfahrposen kommen mit A-05). Fahrt und Zyklus sind in der Zeit hintereinander, mehr nicht.
- Eine Einheit gleichzeitig; kein Quiver; kein `toggle`-Modell; Preload-Ausrichtung und Zeiten sind `schaetzung` (SIM-04).

## Am Roboter zu prüfen
- Nichts; erst echte Zykluszeiten (SIM-08) ersetzen die Schätzwerte.

Hinweis (SIM-23, ADR-033): `toggle` ist inzwischen modelliert, `spiel.toggle_anfang` ist `neutral`.
