# ADR-005: three.js r128 für den Browser-Viewer

Status: angenommen (1. Nov 2026)

## Entscheidung
- tools/viewer/index.html nutzt three.js **r128** und OrbitControls aus `examples/js` (globales `THREE`), per CDN geladen
  wie die Vorlage tools/viewer/reference/frostbyte-lift.html.
- Version fest auf r128. Nicht mischen: ab r148 gibt es `examples/js` nicht mehr, OrbitControls bricht dann.
- Kein Build-Schritt, kein npm. Die Seite wird per Doppelklick (`file://`) geöffnet; Logs und field.json werden
  über Dateiauswahl oder Drag-and-drop geladen (fetch geht unter `file://` nicht zuverlässig).

## Begründung
Gleiche Bibliothek und Version wie die Vorlage, damit Robotermodelle übernommen werden können. Kein Werkzeug nötig
außer einem Browser.

## Folgen
- Ohne Internet (Turnierhalle) lädt three.js nicht → kein 3D. Der Viewer zeigt dann eine Meldung; Ereignisliste und
  Werte funktionieren weiter. Falls nötig später three.min.js r128 lokal unter tools/viewer/vendor/ ablegen (neues ADR).
- Update auf neuere three.js-Version nur mit neuem ADR (Umstieg auf ES-Module).
