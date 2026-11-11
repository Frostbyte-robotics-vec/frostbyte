---
id: CAD-03
phase: "1"
titel: VEX-STEP-Bibliothek automatisch auf jeden Laptop holen (Release-ZIP, Download-Script, /start)
status: blocked
owner: arjanssen
branch: ticket/cad-03
depends: [CAD-02]
hw: nein
---

## Ziel
Alle im Team haben die VEX-Teilebibliothek als STEP (243 Dateien, aus CAD-02) in `cad/lib/`, ohne etwas zu tun: `/start` ruft das Download-Script auf, das beim ersten Mal das ZIP aus einem GitHub-Release lädt und entpackt. `cad/lib/` ist nicht im Git.

## Fertig, wenn …
- `tools/fetch_cad_lib.py` (nur Standardbibliothek): lädt ZIP aus dem Release, prüft SHA-256, entpackt nach `cad/lib/`, tut nichts wenn schon vorhanden (Marker-Datei), ZIP-Pfade ausserhalb des Ziels werden abgelehnt
- Test `tools/test_fetch_cad_lib.py` (lokales ZIP über file://): Erstlauf, zweiter Lauf ohne Download, falsche Prüfsumme, unsicherer ZIP-Pfad
- `cad/lib/` in `.gitignore`; `/start` (`.claude/commands/start.md`) ruft das Script auf; `cad/tools/ANLEITUNG.md` beschreibt es
- Release `vex-step-1.0.0` mit `vex_step_1_0_0.zip` existiert (Mensch gibt frei, siehe Übergabe)
- `python3 tools/board.py validate` grün

## Kontext
Repo ist öffentlich. Weitergabe der VEX-CAD-Bibliothek vorher auf Lizenz prüfen (Übergabe).

## Hardware-Prüfung
—

## Übergabe
BLOCKED: VEX-Lizenz prüfen und Release vex-step-1.0.0 anlegen (Mensch). Code fertig auf Branch ticket/cad-03, Details dort in der Übergabe.
Stand: angelegt.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-10 · arjanssen (KI) · Ticket angelegt und übernommen · –
