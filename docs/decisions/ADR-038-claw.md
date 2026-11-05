# ADR-038: Claw-Subsystem (Strom-Plateau-Erkennung, Cup/Pin-Modus)

Status: angenommen (5. Nov 2026), Ticket M-03.

## Entscheidung
- `lib/subsystems/claw.hpp` (`ClawSubsystem`), reine Logik ohne PROS. Schnittstelle wie Lift/Intake: `request_open`, `request_close(mode)`, `state()`, `result()`, `update(now_s, ClawSensor{position_deg, velocity_deg_s, current_a, ok}) -> ClawOutput{volts}`.
- **Zustände:** `unknown → opening → open → closing → held` (und zurück über `opening`), dazu `error`. Vor dem ersten Öffnen ist die Lage unbekannt: 0 V, `request_close` abgelehnt.
- **Öffnen:** `open_v` bis zum Anschlag (Stillstand trotz Spannung, `open_settle_s`, nach `grace_s`); die Lage dort ist der Nullpunkt. Timeout → `error (open_timeout)`, 0 V.
- **Schließen / Plateau:** `close_v` des Modus. Griff = Backe steht (`stall_deg_s`) **und** Strom ≥ `plateau_a` des Modus, beides `plateau_s` in Folge, erst nach `grace_s` (Anlaufstrom zählt nicht). Danach `held` mit der kleineren `hold_v`. Der Strom allein reicht nicht: er steigt auch am leeren Ende des Wegs.
- **Nichts gegriffen:** Erreicht der Weg `empty_travel_deg − empty_margin_deg` (leeres Ende) oder läuft `close_timeout_s` ab, ist das Ergebnis `missed`; die Claw öffnet von selbst wieder (kein Dauerdruck, Motor nicht überhitzt). Stillstand mit zu kleinem Strom ist kein Griff, sondern läuft ins Timeout.
- **Cup/Pin:** je Modus eigene Schließ-/Haltespannung und Stromschwelle. Der Modus kommt mit dem Befehl; die Entscheidung Cup oder Pin gehört dem Zyklus/RuleGuard (SG6), nicht der Claw.
- **Sicher bei Ausfall:** Motor weg oder NaN → `error (sensor)`, 0 V, Lage unbekannt; erst `request_open` heilt. Ungültige Konfiguration → `error (config)`, Befehle abgelehnt. Spannung ±12 V begrenzt.
- **Strom als Eingang:** `hal::Motor` hat noch kein `current_a()`. Das Subsystem nimmt den Strom über `ClawSensor`; die HAL-Erweiterung (ProsMotor `get_current_draw`, SimMotor) ist ein eigenes Ticket (F-08), damit die Schnittstellenänderung in einem Zug mit ADR-006 passiert.

- **Gesperrt bis gemessen:** `empty_travel_deg` und `plateau_a` haben als Standard NaN und stehen nicht in `config/`. Die Claw ist dann `error (config)` und fährt nicht, wie beim Lift (`lift_top_offset_in`). Sonst würde ein zu großer geratener Leerweg das Schließen ins Leere (hoher Strom am Anschlag) als Griff melden. Die Freigabe geschieht in M-13 mit gemessenen Werten.

## Grenzen / offen
- Die Simulation hat keine Claw/Strommodell; getestet wird gegen eine Teststrecke (Spannung → Geschwindigkeit, Objekt, Strom im Stall).
- „50 Griffe, ≥ 48 erkannt“ ist ein Roboter-Kriterium (Abnahme zusammen mit F-08 und der Verdrahtung in einem Folgeticket).
- Werte in `config/` sind Startwerte (`TODO(HW)`); Tests nehmen eigene. Ob Stacker und Runner einen Greifer haben, ist offen.

## Am Roboter zu prüfen
- `empty_travel_deg`; Stromschwellen aus Logs von 50 Griffen (Cup und Pin getrennt); Halte-Temperatur; ob `stall_deg_s` bei Reibung der Backe stimmt.
