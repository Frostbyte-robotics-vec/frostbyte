# ADR-029: RoutePlayer (lib/match/route_player.hpp)

Status: angenommen (3. Nov 2026), Ticket A-03. Der Advisor (Opus) hat vorgegeben: reiner Steuerfluss, Namen und Koordinaten bleiben beim Executor; jeder Befehl braucht ein Timeout (Standardwerte je Befehlsart in der Konfiguration); `cancel` liefert den Endstatus zurück, damit ein schon gesetzter Zyklus nicht wiederholt wird; Grenzen beim Anlegen prüfen, nicht mitten im Match; Zeitbasis und Teamentscheidungen aufschreiben.

## Entscheidung
- `lib/match/route_player.hpp` (Schicht 1): spielt die flache `RouteCmd`-Liste (ADR-023) Takt für Takt ab. Reine Logik, kein dynamischer Speicher (feste Felder), keine PROS-Header.
- **Der Player kennt keine Koordinaten und kein Spielfeld.** Jeder Befehl, den er nicht selbst erledigt, geht samt Namen an einen `RouteExecutor` (Schnittstelle in derselben Datei: `start(lane, cmd, now_s)`, `poll(lane, now_s)`, `cancel(lane, now_s)`). Der Executor löst Feldnamen (`goto`, `cycle goal=`, `toggle`, `start`) in Posen auf, löst `level=auto` auf und bildet `RouteSource` auf `CycleSource` ab. Der Roboter parst weiter kein JSON: die Daten kommen als erzeugte Tabelle, die der Adapter hält (Ticket A-10; die Simulation liest `field.json` selbst, A-04). Damit ist die offene Frage aus ADR-022/023 beantwortet.
- Der Player erledigt selbst: `target_p10` (nichts), `wait`, `end_hold` (Route fertig), die Blöcke `repeat`, `parallel`, `at`. Alles andere (`start`, `goto_*`, `turn`, `intake`, `cycle`, `toggle`, `signal`, `wait_for`) gehört dem Executor.

## Ablauf
- `begin(now_s)` startet; `update(now_s, match_time_s)` ist der Takt. Zeit kommt immer als Parameter. **Matchzeit** ist `match_time_s`, die Zeit seit Matchbeginn, vom Aufrufer geliefert (nicht seit `begin`). NaN = unbekannt: kein `at` löst aus, und ein erreichtes `at` wartet (sichere Seite; das Match-Ende hält ohnehin alles an, M-07).
- **Lanes:** die Hauptlane läuft die Liste. `parallel` startet pro direktem Kindbefehl eine eigene Lane (die Eltern-Lane wartet); fertig, wenn alle Kinder fertig sind. Die Lane-Nummer ist der Slot beim Executor. `repeat n` hat je Lane einen Zähler-Stack.
- **Grenzen werden beim Anlegen aus der Struktur berechnet** (`status() == invalid` statt Abbruch im Match): höchstens `kMaxLanes` gleichzeitige Lanes (6), `kMaxRepeatDepth` verschachtelte `repeat` je Lane (4), `kMaxAts` `at` (8), `body_len` innerhalb der Grenzen, `end_hold` genau einmal und zuletzt, `at` nur oben mit genau einer Einzelanweisung. Eine ungültige Route startet nichts.
- **Pro Takt höchstens `kMaxStepsPerTick` (64) Befehle** weiterschalten; der Rest verschiebt sich auf den nächsten Takt, nie ein Fehler (Zeitbudget des Steuer-Tasks).

## Timeouts
- Jeder Befehl hat eins: `timeout=` aus der Route, sonst der Standardwert seiner Art aus `RoutePlayerConfig` (Werte in config/stacker.hpp und config/runner.hpp). Da `on_timeout=` nur zusammen mit `timeout=` geparst wird, bedeutet ein Standard-Timeout immer `skip`.
- **Der Player besitzt das Timeout der Route und ruft bei Ablauf `executor.cancel()`.** `cancel` gibt den Endstatus zurück: `done`, wenn das Ziel schon erreicht war (der Adapter meldet beim Zyklus `done`, wenn `result().placed`), sonst `failed`. `on_timeout` greift nur bei `failed`. Dadurch wiederholt `retry` keinen Zyklus, der im RUECKZUG ins Timeout lief (ADR-028). **Der Adapter gibt `timeout_s` nicht in `CycleRequest` weiter**; sonst liefen zwei Timeouts im selben Takt mit verschiedenem Ausgang.
- Meldet der Executor selbst `failed`, gilt dieselbe Regel wie beim Timeout.
- **on_timeout (Teamentscheidung, kein Manual-Satz):**
  - `skip` (Standard): weiter mit dem nächsten Befehl.
  - `retry`: genau **ein** weiterer Versuch, mit frischem Zähler bei jedem Durchlauf des Befehls (also in jedem `repeat`-Durchgang neu); scheitert auch der zweite, wie `skip`.
  - `abort`: die Route endet (`status() == aborted`): alle Lanes werden beim Executor abgebrochen, offene `at`-Wächter sind aus (ein `at 52s goto MIDFIELD` startet danach **nicht**). Mechanismen halten ist Sache des Aufrufers.

## `at <t>s` (Teamentscheidung)
- Ab Start scharf. Ist die Matchzeit >= t und das `at` liegt hinter dem laufenden Top-Level-Befehl, werden alle Lanes beim Executor abgebrochen und die Hauptlane springt zum `at`; sein Befehl läuft wie jeder andere (mit Timeout und `on_timeout`), danach geht es hinter dem `at` weiter. Sind mehrere fällig, gewinnt das mit der höchsten Position (die früheren sind dann übersprungen).
- Erreicht der normale Ablauf ein `at` vor t, **wartet die Route bis t**. Der Befehl läuft nie vor t (z. B. `at 52s goto MIDFIELD_S` soll nicht vor dem Endgame ins Midfield fahren). Das ist eine Team-Festlegung, kein Satz aus dem Game Manual.

## Beobachtung
`status()` (`idle`, `running`, `done`, `aborted`, `invalid`), `current_cmd()` (Befehl der Hauptlane, für die Log-Spalte `route_step`), `failed_steps()` (Zähler der gescheiterten Versuche, auch skip) und `last_failed_line()`.

## Folgen
- ADR-028 "Folgen" gilt angepasst: `auto` auflösen und `RouteSource` abbilden macht der Executor-Adapter (A-10), nicht der Player. Der Adapter ruft `CycleController::cancel` in `cancel(lane)` und meldet `done`, wenn `result().placed`.
- Mirror Rot/Blau, Funk zwischen den Robotern (`signal`/`wait_for`) liegen im Executor.
- Neue Tickets: A-10 (echter Executor-Adapter für den Roboter).
