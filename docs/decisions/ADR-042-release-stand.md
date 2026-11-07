# ADR-042: Release-Stand (Version, Commit, dirty) auf dem Display

Status: angenommen (L-03, 2026-11-07)

## Entscheidung
- Die Version steht in `version.txt` (eine Zeile). Das Makefile liest sie, den kurzen Git-Hash und „dirty“ (`git status --porcelain --untracked-files=no`) und übergibt sie als `-DFB_VERSION`, `-DFB_COMMIT`, `-DFB_DIRTY`. Fallbacks in `src/main.cpp`: Version `?`, Commit leer (Anzeige `-`), dirty = 1 (ein Build ohne Makefile gilt nie als sauber).
- `dirty` ignoriert nicht verfolgte Dateien: Eine neue, noch nicht hinzugefügte `.cpp` wird vom Build gar nicht erfasst (PROS nimmt alle Dateien in `src/`) und könnte also trotzdem im Roboter landen, ohne `dirty` auszulösen. Akzeptiert, weil `git status` mit untracked-Dateien im Repo wegen `Frostbyte_CAD_RevE/` u. Ä. immer dirty wäre; die Release-Checkliste verlangt `git status` sauber.
- Damit `main.cpp` bei neuem Commit/dirty-Wechsel neu übersetzt wird, hängt `bin/<robot>/main.cpp.o` an `bin/<robot>/build_info.<version>_<commit>_<dirty>.stamp`; das Rezept löscht ältere Stempel.
- Format und Kürzung: `lib/util/build_info.hpp` (`format_build_info`), getestet in `test/test_build_info.cpp`. `dirty` ersetzt das Datum und bleibt auch bei Kürzung sichtbar.
- LCD-Zeilen: 0 Version/Commit/Datum, 1 Roboter-Name, 2–3 Takt-Statistik, 4–7 Routinenmenü.
- Ablauf und Checkliste: `docs/release.md`.

## Annahmen (TODO(HW), Ticket L-05)
- Zeile 0 des LLEMU-Displays ist lesbar und wird nicht von Zeile 1 überdeckt; 26 Zeichen passen in eine Zeile.
