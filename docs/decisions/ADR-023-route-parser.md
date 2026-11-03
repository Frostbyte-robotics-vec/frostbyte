# ADR-023: Route-Parser und route2cpp

Status: angenommen (3. Nov 2026), Ticket A-02. Der Advisor (Opus) hat vorgegeben: eine Grammatik, in C++ (die Simulation liest `.route`-Dateien zur Laufzeit); `route2cpp.py` nur als dünner Aufruf (Vorbild `controls_export`); Namen nach Kategorie prüfen, aber keine Koordinaten auflösen; nichts am Format stillschweigend erweitern.

## Entscheidung
- `lib/match/route.hpp`: flache, speicherfreie Befehlstypen (`RouteCmd`, Namen als `char[32]`). Blöcke (`parallel`, `repeat`) und `at` sind eigene Befehle; die folgenden `body_len` Befehle (verschachtelte mitgezählt) gehören dazu. Das spielt der RoutePlayer (A-03) auf dem Roboter ab.
- `sim/route_parser.{hpp,cpp}`: `parse_route(text, RouteNames)` (nur Laptop, benutzt `std::string`), `route_names_from_field`, `route_to_cpp`. Erster Fehler bricht ab: `Zeile N: Meldung`, bei nicht geschlossenem Block die Zeile der öffnenden Klammer. Der ältere `sim::load_route` (nur Zeilen lesen) bleibt bis A-04 stehen.
- `tools/route_export.cpp` (`frostbyte_route_export`) + `tools/route2cpp.py`: erzeugt aus einer Route einen Header mit einem `constexpr RouteCmd`-Feld. Das Python-Skript ruft nur das Programm auf; es gibt keine zweite Grammatik.
- Geprüft werden Namen nach Kategorie (Goal, Loader, Toggle, Pose); **Koordinaten werden nicht aufgelöst** (null-Posen wie `MIDFIELD_S` dürfen die Prüfung nicht beeinflussen). `level` ist eine ganze Zahl 0–5 oder `auto` (ADR-022); `auto` löst der Aufrufer auf.

## Festlegungen, wo das Format offen war
- `start` ist Pflicht, genau einmal, die erste Anweisung (nur `target_p10` darf davor stehen, höchstens einmal). `end hold` ist Pflicht, steht auf oberster Ebene und ist die letzte Anweisung.
- `at <t>s` nur auf oberster Ebene, mit `s` hinter der Zahl, genau eine Einzelanweisung (kein Block, kein `at`/`start`/`end`).
- `turn <deg>` ist eine **absolute** Richtung im Feldrahmen.
- `on_timeout=` nur bei Anweisungen mit `timeout` und nur zusammen mit `timeout=`; `timeout`, `tol`, `wait`, `at`-Zeit und `target_p10` müssen > 0 sein; `repeat` 1–100.
- Ereignisnamen (`signal`, `wait_for`): `a-z`, `0-9`, `_`, höchstens 31 Zeichen; `wait_for <stacker|runner>.<name>`.
- `toggle` ist laut Format „nur Runner“; der Parser kennt den Roboter nicht und prüft das nicht (A-03/A-04).
- `goto <NAME>` akzeptiert jeden Feldpunkt (Goal, Loader, Toggle, Pose); `cycle goal=` nur Goals, `toggle` nur Toggles, `start` nur Posen.

## Grenzen / offen
- Der erste Fehler genügt; eine Liste aller Fehler gibt es nicht.
- Die erzeugten Header (`out/routes/…`) werden noch nirgends in den Roboter-Build eingebunden (A-03).
