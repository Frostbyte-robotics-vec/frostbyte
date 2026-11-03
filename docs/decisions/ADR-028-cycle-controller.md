# ADR-028: CycleController (lib/tasks/cycle_controller.hpp)

Status: angenommen (3. Nov 2026), Ticket C-01. Der Advisor (Opus) hat vorgegeben: reine Logik mit Eingangs- und Ausgangsstruct, keine direkten Subsystem-Aufrufe; Abbruchpfad und Ergebnis für den RoutePlayer (A-03) jetzt festlegen; Timeout nach dem Setzen ist kein Fehlschlag; FEHLER bleibt passiv; Preload und Quiver getrennt beschreiben; Greifer-Versatz als NaN-Standard in der Konfiguration; Übergangstabelle zuerst, dann ein Test je Zeile.

## Entscheidung
- `lib/tasks/cycle_controller.hpp` (Schicht 2): reine Logik, keine PROS-Header, kein dynamischer Speicher. Subsysteme rufen sich nie gegenseitig auf (plan.md); der Controller bekommt pro Takt ein `CycleInputs` (was die Schichten darunter melden) und gibt ein `CycleCommand` zurück (was die aufrufende Schicht an Antrieb, Intake, Greifer und Lift weitergibt). Intake und Claw gibt es noch nicht (M-03/M-04); die Eingänge sind absichtlich boolesche Meldungen ("Einheit im Bumper", "Greifer geschlossen" ...), sodass diese Subsysteme später nur noch verdrahtet werden.
- Aufruf: `request_cycle(CycleRequest, pins, cups, now_s)` startet, `cancel(now_s)` bricht ab, `update(now_s, CycleInputs)` ist der Takt, `state()` und `result()` fragen ab. `request_cycle` gibt `CycleRefusal::none` zurück oder den Grund, warum nicht gestartet wurde; bei einer Ablehnung ändert sich nichts.
- Die Zustände heißen genau wie in docs/log_format.md (BEREIT, AUFNEHMEN, GREIFEN, HEBEN, AM_GOAL, SETZEN, RUECKZUG, FEHLER); `cycle_state_name()` liefert diese Strings. Kein neuer Name (das bräuchte eine neue Log-Version). "7 Zustände" im Ticket sind BEREIT bis RUECKZUG; FEHLER ist der Fehlerzustand dazu.
- `CycleRequest` spiegelt den Routenbefehl `cycle` (lib/match/route.hpp), ohne `match/` einzubinden (Schichten nur nach unten): Goal-Name, `level` 0..5, `CycleSource {preload, quiver}`, `timeout_s` (< 0: nur die Zustands-Timeouts). `level` muss konkret sein: `auto` (-1) löst der RoutePlayer auf, der Controller lehnt es ab (`level_invalid`). `on_timeout=skip|retry|abort` ist Sache des RoutePlayers; der Controller liefert nur das Ergebnis.

## Übergänge
| Von | Auslöser | Nach | Timeout → |
| --- | --- | --- | --- |
| BEREIT / FEHLER | `request_cycle`, source=quiver | AUFNEHMEN | – |
| BEREIT / FEHLER | `request_cycle`, source=preload | GREIFEN | – |
| AUFNEHMEN | `unit_present` (Bumper meldet eine Einheit) | GREIFEN | `pick_timeout_s` → FEHLER |
| GREIFEN | `claw_closed` | HEBEN | `grasp_timeout_s` → FEHLER |
| HEBEN | `lift_at_target` | AM_GOAL | `lift_timeout_s` → FEHLER |
| AM_GOAL | `at_goal` | SETZEN | `approach_timeout_s` → FEHLER |
| SETZEN | `claw_open` (Setzen fertig) | RUECKZUG, Ergebnis `placed` | `place_timeout_s` → FEHLER (`placed` bleibt false) |
| RUECKZUG | `retreat_done` | BEREIT, Ergebnis fertig | `retreat_timeout_s` → BEREIT (`placed`, `timed_out`) |
| jeder laufende Zustand | `cancel` | BEREIT, Ergebnis `cancelled` | – |
| jeder laufende Zustand | `CycleRequest::timeout_s` für den ganzen Zyklus | FEHLER, in RUECKZUG: BEREIT (`placed`, `timed_out`) | – |
| FEHLER | `cancel` | BEREIT | – |

Ein laufender Zyklus lehnt `request_cycle` ab (`busy`); der RoutePlayer ruft vorher `cancel`. Aus FEHLER und BEREIT startet ein neuer Befehl.

## Ausgaben je Zustand
| Zustand | Antrieb | Intake | Greifer | Lift |
| --- | --- | --- | --- | --- |
| BEREIT | stop | aus | halten | halten |
| AUFNEHMEN | zur Quelle | an | halten | halten |
| GREIFEN | stop | aus | schließen | halten |
| HEBEN | stop | aus | schließen | Setzhöhe |
| AM_GOAL | zum Goal | aus | schließen | Setzhöhe |
| SETZEN | stop | aus | öffnen | Setzhöhe |
| RUECKZUG | zurück | aus | halten | `retreat_lift_in` (NaN: halten) |
| FEHLER | stop | aus | halten | halten |

"Halten" des Lifts heißt Ziel NaN, also: aktuelle Höhe halten (`request_hold`). FEHLER ist **passiv**: Antrieb steht, Lift hält, Greifer bleibt, kein automatischer Rückzug. Heraus kommt man nur mit einem neuen Befehl oder `cancel`.

## Ergebnis für den RoutePlayer
`CycleResult { finished, placed, cancelled, timed_out, timeout_state }` gilt, bis der nächste Befehl kommt. `ok()` = `placed` und nicht `cancelled`. Der Fall "Timeout im RUECKZUG" ist **kein Fehlschlag**: das Objekt ist gesetzt (`placed`), nur der Rückzug lief ins Timeout (`TIMEOUT:RUECKZUG` im Log). Sonst würde `on_timeout=retry` den ganzen Zyklus wiederholen, obwohl schon gesetzt wurde. Ein Timeout im SETZEN zählt als nicht gesetzt (ob der Greifer wirklich öffnete, ist unbekannt); das ist die vorsichtige Lesart. `timeout_state` ist der Zustand, in dem das Timeout auslöste, für `TIMEOUT:<zustand>`.

## Preload und Quiver
- **preload:** die Einheit ist schon im Besitz. AUFNEHMEN entfällt, Start in GREIFEN (schließt den Greifer, falls noch nicht zu). `may_grab` wird **nicht** geprüft: der Besitz enthält die Einheit schon, die Prüfung würde immer ablehnen.
- **quiver:** Start in AUFNEHMEN. Welche Art Einheit (Pin oder Cup) aus dem Quiver kommt, steht nicht in der Route; der Controller verlangt deshalb Platz für mindestens eine Art (`may_grab(pin) || may_grab(cup)`), und bei unbekanntem Besitz (< 0) lehnt er ab (`cannot_grab`). Die Prüfung der tatsächlich gegriffenen Art bleibt bei M-04 (Intake meldet den Besitz, SG6 über `may_grab`).

## Ablehnungsgründe (`CycleRefusal`)
`busy`, `goal_unknown` (Name nicht im GoalBook), `level_invalid` (nicht 0..5, auch `auto`), `level_unknown` (Höhe nicht bekannt, z. B. `stufe_in` fehlt: C-06), `offset_unknown` (Greifer-Versatz NaN), `too_high` (Setzhöhe über der weichen Decke des RuleGuard, SG3), `cannot_grab` (quiver und kein Platz oder Besitz unbekannt), `bad_config` (Timeouts nicht > 0 oder GoalBook ungültig). Ablehnen beim Start gibt eine klare Meldung statt eines Timeouts im HEBEN.

## Höhe
Lift-Zielhöhe = `GoalBook::place_height_in(goal, level)` + `CycleConfig::grip_offset_in` (Greifer gegenüber gemessener Lifthöhe; TODO(HW), Standard NaN, geraten wird nichts). Gegen `RuleGuard::soft_lift_ceiling_in()` geprüft, bevor der Zyklus startet.

## Folgen
- `CycleConfig` (Zustands-Timeouts, `retreat_lift_in`, `grip_offset_in`) steht in `config/stacker.hpp`. Die Zeiten sind Startwerte (Software-Entscheidung, am Roboter zu prüfen); `grip_offset_in` ist offen. Der Runner bekommt nichts, bis geklärt ist, ob er überhaupt Zyklen fährt (siehe config/runner.hpp, Lift-TODO).
- Der Controller ist noch nicht in `src/main.cpp` verdrahtet (Intake, Claw, Port-Zuordnung fehlen: M-03, M-04, F-07).
- Der Executor-Adapter (A-10, ADR-029) löst `auto` auf, bildet `RouteSource` auf `CycleSource` ab, ruft `CycleController::cancel` aus `RouteExecutor::cancel` und meldet `done`, wenn `result().placed`; er gibt `timeout_s` nicht weiter (der RoutePlayer besitzt das Timeout der Route).
- Die Eingänge sind Pegel und gelten ab dem ersten Takt nach dem Zustandswechsel. Ein veraltetes Flag (z. B. `at_goal` vom letzten `goto`, `lift_at_target` von der alten Höhe) würde den Zustand nach 10 ms überspringen. Beim Verdrahten muss jede Meldung an den aktuellen Befehl gebunden sein (erst melden, wenn der Befehl dieses Zustands angenommen wurde).
