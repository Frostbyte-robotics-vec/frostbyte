---
id: CAD-02
phase: "1"
titel: Fusion-Script: VEX-Teilebibliothek (F3D) per Batch nach STEP exportieren
status: in_progress
owner: arjanssen
branch: ticket/cad-02
depends: []
hw: nein
---

## Ziel
Die VEX-Teilebibliothek (243 F3D-Dateien in Unterordnern) wird mit einem Fusion-Script in einem Lauf nach STEP exportiert, Ordnerstruktur bleibt erhalten. So lassen sich Teile und Baugruppen ins Repo bzw. zu Claude hochladen (F3D ist nicht lesbar).

## Fertig, wenn …
- `cad/tools/ExportStep/ExportStep.py (+ .manifest)` im Repo: Quellordner und Zielordner wählbar, Unterordner werden gespiegelt, Fehler pro Datei werden gesammelt (ein Fehler bricht den Lauf nicht ab), Protokoll `export_log.txt` im Zielordner
- `cad/tools/ANLEITUNG.md`: Einrichten und Ausführen in Fusion
- Lauf in echtem Fusion: Anzahl exportierter STEP-Dateien = 243 abzüglich im Protokoll genannter Fehler; Ergebnis im Ticket
- `python3 tools/board.py validate` grün

## Kontext
Quelle: ~/Downloads/fusion_parts_library_1_0_0 (243 F3D, 211 MB). Ein Test des Scripts ist nur in Fusion möglich (CI prüft nur die Syntax).

## Hardware-Prüfung
—

## Übergabe
Stand: Script, Manifest und ANLEITUNG geschrieben, Syntax geprüft. Nächster Schritt: Lauf in echtem Fusion (erst 2–3 Dateien, dann alle 243), Ergebnis hier eintragen.
Am echten Rechner offen: Script ist ungetestet (API-Aufrufe documents.open/createSTEPExportOptions), `export_log.txt` prüfen.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-10 · arjanssen (KI) · Ticket angelegt und übernommen · –
2026-11-10 · arjanssen (KI) · cad/tools/ExportStep + ANLEITUNG geschrieben · –
