# VEX-Teilebibliothek als STEP (CAD-03)

Die exportierte Bibliothek (243 STEP-Dateien aus der VEX-CAD-Fusion-Bibliothek, Export mit `cad/tools/ExportStep`, CAD-02)
liegt als ZIP im GitHub-Release `vex-step-1.0.0`.

- `/start` holt sie automatisch nach `cad/lib/`; von Hand: `python3 tools/fetch_cad_lib.py`.
- Der Download passiert nur beim ersten Mal (ca. 117 MB, entpackt ca. 512 MB). Der Ordner `cad/lib/` ist nicht im Git.
- Das Script prüft die SHA-256-Prüfsumme und entpackt nichts, wenn sie nicht stimmt.
- Kaputter oder unvollständiger Ordner: `cad/lib/` löschen und das Script erneut starten.

## Neue Fassung veröffentlichen
1. Mit `ExportStep` neu exportieren, Ordner zu einem ZIP packen (Dateien direkt im ZIP-Stamm, ohne Oberordner).
2. ZIP als Anhang an einen neuen Release hängen (Tag z. B. `vex-step-1.1.0`).
3. `URL` und `SHA256` (`shasum -a 256 datei.zip`) in `tools/fetch_cad_lib.py` ändern; Ticket anlegen, Test läuft mit.
