# Release-Ablauf vor einem Turnier (L-03)

Quelle: docs/plan.md, Abschnitt „Release-Ablauf“. Die Prüfung am echten Brain steht in Ticket L-05.

## Version, Commit, Datum
- Version steht in `version.txt` (eine Zeile, z. B. `1.2.0`). Beim Release dort erhöhen und committen.
- Das Makefile liest beim Build `version.txt`, den kurzen Commit-Hash und ob ungesicherte Änderungen da sind
  (`FB_VERSION`, `FB_COMMIT`, `FB_DIRTY`). Ohne git bleibt der Commit leer, die Anzeige zeigt `-`.
- Das Brain zeigt auf Display-Zeile 0: `v<Version> <Commit> <Datum>`; bei ungesicherten Änderungen statt des Datums `dirty`.
  Format und Kürzung: `lib/util/build_info.hpp` (Test: `test/test_build_info.cpp`).
- **Ein Release-Build ist nie `dirty`.** Zeigt das Display `dirty`, ist der Stand nicht reproduzierbar: nicht hochladen.
- Die Defines ändern keine Quelldatei, `make` baut deshalb nicht von selbst neu. Vor jedem Release: `make clean`.

## Checkliste
1. [ ] Code-Freeze 3 Tage vorher; seither nur Bugfixes
2. [ ] `main` aktuell, alle Pull Requests dieses Releases gemergt, Arbeitsverzeichnis sauber (`git status`)
3. [ ] Format ok: `python3 tools/format.py --check`
4. [ ] Laptop-Tests grün: `cmake -S test -B build-test && cmake --build build-test && ctest --test-dir build-test`
5. [ ] `python3 tools/board.py validate` grün
6. [ ] `version.txt` erhöht, committen, Tag setzen (z. B. `v1.2-quali-feb`), Tag pushen
7. [ ] `make clean`, dann `make ROBOT=stacker` und `make ROBOT=runner` bauen (beide fehlerfrei)
8. [ ] Hardware-Checkliste: alle Routinen 10× fehlerfrei, Bench-Test bestanden, Log-Speicher (SD-Karte) leer
9. [ ] **Slot 2 (Fallback) belegen:** Den bisherigen, bewährten Release in Slot 2 hochladen, *bevor* Slot 1 überschrieben wird
10. [ ] **Slot 1:** neuen Release hochladen (`pros upload --slot 1` je Roboter, Port nach Projektgewohnheit)
11. [ ] Am Brain prüfen: Display zeigt die erwartete Version und den Commit, **nicht** `dirty`; Menü zeigt die Routine
12. [ ] Version, Commit und Slot-Belegung in die Übergabe/Turnierliste eintragen

## Rückfall
- Verhält sich der neue Stand am Turnier falsch oder ist unklar: Slot 2 starten (vorheriger Release). Display kontrollieren: Version
  muss die des Fallbacks zeigen.
- Danach nicht am Turnier neu bauen und hochladen, sondern Fehler als Issue erfassen.

## Nach dem Turnier
Logs von der SD-Karte sichern, SD-Karte leeren, Probleme als Issues erfassen.
