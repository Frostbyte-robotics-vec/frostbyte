---
id: SIM-16
phase: "1b"
titel: Log-Tab für die Live-Fahrt: laufende Ereignisliste, Filter, CSV-Export im Log-Format v1
status: todo
owner:
branch: ticket/sim-16
depends: [SIM-13, SIM-15]
hw: nein
---

## Ziel
Der Log-Tab auf /sim/ zeigt während der Live-Fahrt laufend, was passiert: Zeitstempel, Roboter, Ereignis (COLLISION, BLOCKED, Ausgaben/„output“, Hinweise wie Ersatz-Startpose). Die Fahrt lässt sich als CSV speichern und danach im normalen Viewer abspielen.

## Fertig, wenn …
- Neue Einträge erscheinen sofort oben bzw. unten mit automatischem Mitscrollen (abschaltbar), Zähler je Ereignisart, dieselben Filter-Chips wie im Viewer
- Knopf „Log leeren“ und „CSV speichern“; die CSV entspricht docs/log_format.md v1 (Kopfzeile, Spalten, log_ms-Raster) – geprüft mit logparse.js im Node-Test (Export → Parser → gleiche Posen und Ereignisse)
- Die gespeicherte CSV lässt sich in /viewer/ und /sim/ per Datei-Auswahl laden
- Unbekannte Token werden angezeigt, aber nie verworfen (Regel aus log_format.md)
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün

## Kontext
Vom Menschen gewünscht (3.11.): „Log für Kollision erkannt, Ausgabe erkannt usw.“ Offene Frage an den Menschen: Was genau ist mit „output detected“ gemeint (z. B. PLACED beim Setzen, Motorausgabe, Sensorauslösung)? Bis zur Antwort: COLLISION, BLOCKED und Hinweise; weitere Token kommen ohne Formatänderung dazu. Regeln: CLAUDE.md.

## Hardware-Prüfung
—

## Übergabe
—

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-03 · DAVIDCOPCOP (KI) · Ticket angelegt · –
