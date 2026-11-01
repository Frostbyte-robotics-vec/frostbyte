# ADR-002: Formatierung mit clang-format

Status: angenommen (1. Nov 2026)

## Entscheidung
- `.clang-format` im Repo-Root (Google-Basis, 4 Leerzeichen, 100 Spalten).
- `python3 tools/format.py` formatiert, `python3 tools/format.py --check` prüft.
- CI (`.github/workflows/format.yml`) führt die Prüfung bei jedem PR aus, clang-format 18.1.8 fixiert.
- Neues Paket: `clang-format` (pip), nur als Entwickler- und CI-Werkzeug, nicht im Roboter-Build.

## Begründung
Einheitlicher Stil ohne Diskussion im Review; feste Version, damit lokal und CI gleich formatieren.

## Folgen
Wer C++ committet, formatiert vorher. Weicht die lokale clang-format-Version ab, gilt die CI-Version.
