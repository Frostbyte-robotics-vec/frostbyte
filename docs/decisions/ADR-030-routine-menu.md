# ADR-030: Routinenmenü und gespeicherte Auswahl (lib/match/routine_menu.hpp)

Status: angenommen (3. Nov 2026), Ticket A-06. Der Advisor (Opus) hat vorgegeben: A-06 liefert Menülogik, Speichern und Laden ohne Hardware; "Route von SD" ist eine eigene Aufgabe, weil der Parser nur auf dem Laptop läuft; Dateimuster wie bei den Tuning-Werten (ADR-020).

## Entscheidung
- **`RoutineMenu`** (Schicht 1, reine Logik, kein PROS): Tasten `next`/`prev` (laufen im Kreis) und `confirm`, Ausgabe ist der gewählte Name. Anzeige und Berührung gehören in src/ (Ticket A-11).
- **Katalog** `routine_catalog` aus docs/plan.md ("Routinen-Katalog"): `safe_red`, `safe_blue`, `awp_red`, `awp_blue`, `skills`, `test_square`, `nothing`. Gleich für beide Roboter, deshalb nicht in config/. Der Standard ist `nothing` (stehen bleiben): gilt bei fehlender, kaputter oder unbekannter Datei.
- **Datei** `/usd/routine.txt`, Format v1: Kopfzeile `# frostbyte-routine v1`, dann `routine=<name>\n`. Gespeichert wird der Name, nicht die Nummer, damit sich der Katalog ändern darf. Ganz oder gar nicht: falsche Version, fehlender, doppelter oder unbekannter Schlüssel, Name nicht im Katalog, Zeichen hinter dem Namen, fehlendes Zeilenende (Stromverlust) → `invalid`, Auswahl bleibt. Änderung am Format nur mit neuer Versionsnummer.
- **Laden/Speichern** wie ADR-020: einmal in `initialize()` laden; speichern nur im niedrig priorisierten UI-Task, nur nach `confirm` (`take_save_request()`), nie im Steuer-Task. Schlägt das Schreiben fehl, bleibt die Auswahl im Speicher.
- `LoadResult` liegt jetzt in `hal/interface/storage.hpp` (vorher `tuning_store.hpp`), weil beide Speicher-Nutzer es brauchen.

## Nicht Teil dieses Tickets
- **Route von SD (Trainingsmodus):** Der Parser (`sim/route_parser.hpp`) benutzt `std::string` und `std::vector` und läuft nur auf dem Laptop (ADR-023); Namen werden erst im Executor-Adapter aufgelöst (A-10, ADR-029). Eine Route zur Laufzeit zu laden geht deshalb erst nach A-10 und braucht einen Parser in `lib/` ohne dynamischen Speicher. Das ist eine Schnittstellenänderung und bekommt ein eigenes Ticket mit eigenem ADR (A-12); der Parser wird nicht kopiert.
- Anzeige auf Brain und Controller, echte SD-Karte, Neustart am Roboter: A-11. Controller-Anzeige der Routine: C-04.
- Verdrahtung in `src/main.cpp` (Laden in `initialize()`, UI-Task): A-11.
