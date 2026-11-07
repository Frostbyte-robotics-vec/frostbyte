# ADR-040: CAD als Fusion-360-Script im Repo

Status: angenommen (CAD-01, 2026-11-07)

## Entscheidung
- Der Runner wird nicht als Fusion-Datei, sondern als Python-Script für die Fusion-API gepflegt: `cad/runner/FrostbyteRunner.py`
  baut das Modell aus der VEX-CAD Fusion 360 Library 2.0.0, `cad/runner/pruefung.py` prüft es. Scripts sind diffbar, im Review
  lesbar und erzeugen das Modell jederzeit neu; die Bibliothek selbst liegt nicht im Repo (Pfad `LIB` im Script).
- Einheiten wie in der Software: Zoll und Grad. Koordinaten X rechts, Y vorn, Z oben; Ursprung Mitte zwischen den Antriebsrädern.
  Software-Koordinaten (ADR-012: x vorn, y links) ergeben sich als x = Y, y = −X.
- Teile aus der Bibliothek werden nur platziert, nie verändert. Nicht vorhandene Teile (gekürzte Wellen, gekürzte Platten,
  Druckteile) erzeugt das Script selbst, mit Material im Namen.
- Jede Schraube trägt eine Montagestufe (Attribut `frostbyte/stufe`); die Prüfung testet für jede Schraube, ob ein Werkzeug sie in
  dieser Reihenfolge erreicht.
- Maße aus dem CAD sind Startwerte, keine Messwerte: Sie gehen nach `config/` bzw. `sim/sim_params.json` nur mit `TODO(HW)` bzw.
  `quelle: "CAD-01 ..."` und werden am Roboter ersetzt (M-14, M-11).

## Folgen
- Änderungen am Roboter zuerst im Script, dann `pruefung.py` laufen lassen; Ergebnis (Startvolumen, Kollisionen, Schrauben) in
  den PR schreiben.
- Ein Lauf braucht Fusion 360 auf Windows/macOS; die CI prüft die Scripts nicht.
- Stacker folgt im selben Aufbau (`cad/stacker/`), sobald er nachgebaut wird.
