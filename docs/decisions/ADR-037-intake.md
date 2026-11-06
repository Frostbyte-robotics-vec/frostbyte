# ADR-037: Intake-Subsystem (Lauf bis Bumper, Stau-Erkennung, Rückwärtsimpuls)

Status: angenommen (5. Nov 2026), Ticket M-04.

## Entscheidung
- `lib/subsystems/intake.hpp` (`IntakeSubsystem`), reine Logik ohne PROS. Schnittstelle wie Lift: `request_run`, `request_stop`, `state()`, `update(now_s, IntakeSensor{bumper_pressed, velocity_deg_s, ok}) -> IntakeOutput{volts}`.
- **Zustände:** `idle → running → holding`, bei Stau `running → reversing → running`, dazu `error`. Im Leerlauf und im Fehler 0 V.
- **Lauf bis Bumper:** `running` endet, sobald der Bumper entprellt (`bumper_debounce_s`) gedrückt ist; `holding` mit `unit_present()`. Löst der Bumper in `holding` entprellt, geht der Intake nach `idle`. Das Zählen von Pins/Cups bleibt beim RuleGuard/Zyklus (SG6), der Intake meldet nur „Einheit im Bumper“.
- **Stau:** Rollen langsamer als `stall_deg_s` für `jam_s` in Folge, nach einer Anlaufzeit `grace_s` (nach Start und nach jedem Impuls neu). Bewusst nur aus der Geschwindigkeit, nicht aus dem Strom: `hal::Motor` bleibt unverändert, Strom wäre eine Schnittstellenerweiterung ohne Messwerte (Schwelle wäre geraten).
- **Rückwärtsimpuls:** `reverse_v` für `reverse_s`, dann wieder `running`. Nach `max_retries` Impulsen ist der nächste Stau `error (jam)`, Rollen aus. Ein neues `request_run` setzt Fehler und Zähler zurück.
- **Timeouts:** `run_timeout_s` gilt für den ganzen Lauf einschließlich der Impulse (nicht je Zustand), danach `idle` und `timed_out() == true`; der Impuls hat `reverse_s`. `holding` hat kein Timeout (Normalzustand).
- **Sicher bei Ausfall:** Motor nicht erkannt oder Geschwindigkeit NaN → `error (sensor)`, 0 V. Ungültige Konfiguration (Vorzeichen von `run_v`/`reverse_v`, Zeiten ≤ 0) → `error (config)`, `request_run` gibt `false`.
- Die Spannung wird auf ±12 V begrenzt. Richtung der Rollen bestimmt das Port-Vorzeichen in `config/` (F-07).

## Grenzen / offen
- Stau-Modell der Simulation: siehe Nachtrag SIM-28.
- Verdrahtung an die HAL und in den Steuer-Task: nach F-07 (Ports) und M-10.
- Werte in `config/` sind Startwerte (`TODO(HW)`), Tests nehmen eigene.
- Ob der Runner einen Intake hat, ist offen; beide Konfigurationen sind gleich vorbelegt.

## Am Roboter zu prüfen
- Stau-Schwelle und -Zeit gegen echte Staus; Impulsstärke und -dauer; Bumper-Polarität und Entprellzeit; dass `grace_s` den Anlauf deckt.

## Nachtrag SIM-28: Stau-Modell der Simulation
- `sim/intake_model.*` (`IntakeRollers`): Rollengeschwindigkeit aus der Spannung (`max_deg_s` bei 12 V). Ein Lauf aus dem Stand (Spannung 0 → positiv) klemmt mit der Wahrscheinlichkeit `klemmt`; bei Vorwärtsspannung stehen die Rollen dann. Jeder Rückwärtsimpuls löst den Stau mit `loest_nach_impuls` (0 = nie, 1 = immer). Nur ein Rückwärtsimpuls löst einen Stau, Stopp und Neustart nicht (die Ziehung beim Start wirkt nur, wenn noch nichts klemmt). Nach einem Impuls zieht der Neustart keine neue Klemm-Ziehung, sonst käme ein gelöster Stau sofort zurück.
- Zahlen nur in `sim/sim_params.json` unter `ereignisse.intake_stau`, `quelle: schaetzung` bis zur Messung (M-05). Rng pro Modell mit Seed vom Aufrufer: gleicher Seed = gleiche Folge.
- Tests (`test/test_sim_intake_jam.cpp`) nehmen nur 0, 1 und 0.5 als eigene Wahrscheinlichkeiten und hängen nicht an den Platzhalterwerten: gelöster Stau → Lauf bis Bumper; nie gelöst → `error (jam)` nach `max_retries + 1` Staus, 0 V.

## Nachtrag SIM-31: Stau im Match-Sim
- `Game::start_intake` lässt das echte `IntakeSubsystem` (Regelwerte aus `config/<roboter>.hpp`, von `frostbyte_sim` in `GameSetup::intake_cfg` gesetzt) im 10-ms-Takt gegen `IntakeRollers` laufen, bis die Rollen wieder ziehen (Verzug in ms, 0 ohne Stau) oder der Fehler `jam` kommt (Aufnahme scheitert, nichts in der Hand). Die Dauer der Aufnahme selbst (`ObjectModel::pick`) bleibt unverändert und wird um den Verzug verlängert; ihre Ziehungen sind mit und ohne Stau gleich.
- Eigene Rng-Ströme (Seed des Roboters ^ Konstante), nicht die des Objektmodells. Modell aus, wenn `jam_prob` = 0 oder `intake_cfg[robot].run_v` = 0 (Tests ohne Stau, `GameSetup` ohne Config).
- Log-Token (keine Formatänderung, Version bleibt v1): `INTAKE:JAM` (Stau erkannt), `INTAKE:IMPULSE` (Rückwärtsimpuls beginnt), `INTAKE:ERROR_JAM` (Fehler nach `max_retries + 1` Staus), `TIMEOUT:INTAKE` (Lauf in sein Timeout). Sie tragen die Zeit, zu der sie eintreten; `cancel` verwirft spätere, nach Matchende wird nichts geloggt.
- **Nur Routenbefehl `intake`:** Szenario-Zyklen (Demo, `GameSetup::cycles`) klemmen nie.
- Rollen und Subsystem sehen keinen Bumper: die Aufnahmezeit steckt in `ObjectModel::pick`, der Bumper wird nach dem Stau als erreicht angenommen.
- Unterschied zum Roboter: `run_timeout_s` begrenzt in der Simulation nur die Stauphase; die Aufnahmezeit (`pick`) kommt danach dazu. Zwei Staus plus langsame Bodenaufnahme können so über 4 s liegen und zählen als ok; die Gesamtgrenze setzt der Routen-Timeout (`intake_s`).
- Rat des Advisors (SIM-31), der die Form prägte: eigener Rng-Strom statt des Objektmodells, Stau nur als Verzug (Aufnahmezeit unverändert, mit Wahrscheinlichkeit 0 byte-gleich zu vorher), Ereignisse mit Zeitstempel in einer Warteschlange je Roboter.
- Die JS-Live-Sim (`tools/viewer/livesim.js`) kennt keinen Intake und ist davon nicht betroffen.
