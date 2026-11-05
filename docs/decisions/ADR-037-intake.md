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
- Die Simulation hat kein Stau-Modell; getestet wird gegen eine kleine Strecke im Test (Spannung → Geschwindigkeit, klemmt auf Wunsch). „Stau im Sim“ aus dem Ticket-Kriterium braucht ein Folgeticket.
- Verdrahtung an die HAL und in den Steuer-Task: nach F-07 (Ports) und M-10.
- Werte in `config/` sind Startwerte (`TODO(HW)`), Tests nehmen eigene.
- Ob der Runner einen Intake hat, ist offen; beide Konfigurationen sind gleich vorbelegt.

## Am Roboter zu prüfen
- Stau-Schwelle und -Zeit gegen echte Staus; Impulsstärke und -dauer; Bumper-Polarität und Entprellzeit; dass `grace_s` den Anlauf deckt.
