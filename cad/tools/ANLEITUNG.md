# STEP-Batch-Export (CAD-02)

Wandelt alle F3D-Dateien eines Ordners samt Unterordnern in STEP um (die Ordnerstruktur bleibt erhalten).

## Einrichten (einmal)
1. Fusion 360 starten.
2. Utilities → Add-Ins → Scripts and Add-Ins (Umschalt+S).
3. Reiter „Scripts“ → grünes „+“ → Ordner `cad/tools/ExportStep` wählen → `ExportStep` erscheint in der Liste.

## Ausführen
1. `ExportStep` wählen → „Run“.
2. Quellordner wählen (z. B. `~/Downloads/fusion_parts_library_1_0_0`, 243 Dateien).
3. Zielordner wählen (leerer neuer Ordner, z. B. `~/Downloads/vex_step`).
4. Bestätigen und warten; der Fortschrittsbalken läuft. Abbrechen ist möglich.
5. Am Ende zeigt ein Fenster die Zahl der Exporte; Fehler pro Datei stehen in `export_log.txt` im Zielordner.

## Hinweise
- Die Quelldateien werden nie gespeichert oder verändert.
- Läuft eine Datei in einen Fehler, geht es mit der nächsten weiter.
- Erst mit wenigen Dateien testen: einen Quellordner mit 2–3 F3D-Dateien wählen.
- Das Ergebnis (nicht die F3D) kann in einen Ordner des Repos oder zu Claude hochgeladen werden; bei großen Mengen vorher Größe prüfen (Git-Limit pro Datei 100 MB).
