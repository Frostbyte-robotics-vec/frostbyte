# ADR-002: Formatierung mit clang-format

Status: angenommen (1. Nov 2026)

## Entscheidung
- `.clang-format` im Repo-Root (Google-Basis, 4 Leerzeichen, 120 Spalten, kurze Funktionen und case-Zeilen einzeilig, Kommentare nicht umbrechen (passt zum bestehenden Code)).
- `python3 tools/format.py` formatiert, `python3 tools/format.py --check` prüft.
- CI (`.github/workflows/format.yml`) führt die Prüfung bei jedem PR aus, clang-format 18.1.8 fixiert.
- Neues Paket: `clang-format` (pip, lokal `pip install clang-format==18.1.8`), nur als Entwickler- und CI-Werkzeug, nicht im Roboter-Build.

## Begründung
Einheitlicher Stil ohne Diskussion im Review; feste Version, damit lokal und CI gleich formatieren.

## Folgen
Wer C++ committet, formatiert vorher. Lokal immer `pip install clang-format==18.1.8` verwenden; weicht die Version ab, gilt die CI-Version.

## Nachtrag (1. Nov 2026)
Spaltenlimit 120 statt 100, weil der Code aus den Sim-Tickets so geschrieben ist. Der Bestand ist noch nicht ganz konform (Kommentar-Ausrichtung, src/main.cpp mit Tabs): einmaliges `python3 tools/format.py` über das Repo, sobald die offenen Sim-PRs gemergt sind (Konflikte vermeiden).
