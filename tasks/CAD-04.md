---
id: CAD-04
phase: "1"
titel: VEX-STEP-Download wieder entfernen (Team hat die Bibliothek per WhatsApp)
status: in_progress
owner: arjanssen
branch: ticket/cad-04
depends: [CAD-03]
hw: nein
---

## Ziel
CAD-03 ist unnötig geworden: die STEP-Bibliothek wurde per WhatsApp verteilt, alle haben sie. Download-Script, Test, `/start`-Aufruf und Beschreibung fliegen wieder raus.

## Fertig, wenn …
- `tools/fetch_cad_lib.py`, `tools/test_fetch_cad_lib.py`, `cad/BIBLIOTHEK.md` gelöscht
- `/start` (`.claude/commands/start.md`) ruft kein Download-Script mehr auf; `cad/tools/ANLEITUNG.md` ohne Abschnitt „Ergebnis ans Team verteilen“
- `cad/lib/` bleibt in `.gitignore` (lokale Kopie der Bibliothek, nie ins Git)
- keine Verweise auf `fetch_cad_lib` mehr im Repo (ausser Verlauf in tasks/CAD-03.md)
- Format ok, Laptop-Tests grün, `python3 tools/board.py validate` grün

## Kontext
Das GitHub-Release `vex-step-1.0.0` bleibt vorerst bestehen; Löschen entscheidet der Mensch (VEX-Dateien auf öffentlichem Repo).

## Hardware-Prüfung
—

## Übergabe
Noch nicht begonnen.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-11 · arjanssen (KI) · Ticket angelegt · –
