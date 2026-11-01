# ADR-003: PROS-Projekt im Repo-Wurzelverzeichnis, feste Versionen

Status: angenommen (1. Nov 2026), Ticket S-01

## Entscheidung
- Das PROS-Projekt liegt im Wurzelverzeichnis (`project.pros`, `Makefile`, `common.mk`, `firmware/`, `include/`, `src/`), damit `make ROBOT=stacker|runner` wie in CLAUDE.md funktioniert.
- Feste Versionen: PROS-Kernel **4.2.2**, LemLib **0.5.6** (verlangt Kernel `^4.2.1`), liblvgl **9.2.0** (Brain-Display, Build-Datum und Version anzeigen). Alle drei werden mit eingecheckt (`include/`, `firmware/`), damit ein Build ohne Netz und ohne `pros conduct apply` gelingt.
- LemLib-Header dürfen nur in `hal/pros/` und `src/` eingebunden werden, nie in `lib/` (ADR-001: LemLib hinter eigener Schnittstelle).
- `ROBOT` hat keinen Standardwert; ohne gültigen Wert bricht `make` ab (außer `make clean`).
- Je Roboter eigene Verzeichnisse: `bin/<robot>/` und `.d/<robot>/`. Sonst würde `make ROBOT=runner` nach `make ROBOT=stacker` Objekte mit dem falschen `-DFROSTBYTE_ROBOT_*` wiederverwenden.
- Das Makefile setzt `-DFROSTBYTE_ROBOT_STACKER` bzw. `-DFROSTBYTE_ROBOT_RUNNER`; `config/robot.hpp` wählt daraus `config/stacker.hpp` oder `config/runner.hpp`.

## Abweichungen von der PROS-Vorlage
- `common.mk`: eine Zeile, `DEPDIR := .d` wurde zu `DEPDIR ?= .d`, damit das Makefile ein Verzeichnis je Roboter setzen kann. Nach `pros conduct upgrade` prüfen, ob die Änderung noch da ist.
- Das Beispielverzeichnis `static/` ist entfernt. `firmware/hot-cold-asset.mk` setzt `bin/` fest im Pfad voraus und bricht mit `BINDIR=bin/<robot>`. Sobald Assets eingebettet werden sollen (z. B. LemLib-Pfade), muss diese Regel angepasst werden.

## Folgen
- `tools/format.py` (S-04) durchsucht `include/`; dort liegen fremde Header (pros, liblvgl, lemlib, fmt). Beim Zusammenführen von S-04 und S-01 muss `include/` aus der Format-Prüfung ausgenommen werden (ausgenommen sein sollten auch `firmware/` und alles fremde unter `include/`).
- `project.pros` enthält absolute Pfade der Maschine, die es erzeugt hat (`location`). `make` nutzt die Datei nicht; nur `pros`-Befehle lesen sie.
