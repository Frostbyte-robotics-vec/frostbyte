---
id: SIM-16
phase: "1b"
titel: Log-Tab für die Live-Fahrt: laufende Ereignisliste, Filter, CSV-Export im Log-Format v1
status: done
owner: DAVIDCOPCOP
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
Stand: fertig, PR folgt. Aufnahme + CSV-Export in livesim.js, Log-Tab (Chips mit Zählern, Mitscrollen, Log leeren, CSV speichern, Log laden) in index.html, ADR-025 Nachtrag. Node-Tests (Export → logparse.js) grün. Nicht im echten Browser gesehen (Syntax per node --check). Offen an den Menschen: was „Ausgabe erkannt“ heißen soll; bis dahin COLLISION, BLOCKED, NOTE.

**KI-Review PR #45 (arjanssen/Sonnet, 2026-11-03): Empfehlung ändern.**
- `restartLog()` (livesim.js:252) ruft `record()` zum aktuellen Schritt auf. „Log leeren“ mitten im Raster (z. B. nach 105 Schritten) erzeugt als erste Zeile t_s = 0.105 statt auf dem log_ms-Raster; Fertig-Kriterium „log_ms-Raster“ und die ADR-Aussage „einzige Zeile abseits des Rasters ist die Schlusszeile“ stimmen dann nicht. Vorschlag: in restartLog nur bei `stepNo % logEvery === 0` aufzeichnen (R/Start sind bei 0 immer drauf) plus Test, der bei einem Nicht-Vielfachen leert.
- Hinweise (kein Blocker): x_in/y_in sind die wahre Pose (log_format: Odometrie); Kommentar zu logEvent steht zwischen takeEvents-Kommentar und takeEvents; Browser-Teile (Chips, Zähler, Mitscrollen, Datei laden) nicht im echten Browser gesehen.
- Konflikt mit PR #46 (SIM-17, ebenfalls arjanssen): nur ADR-025 (beide hängen einen Nachtrag an); wer später merged, rebased.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-03 · DAVIDCOPCOP (KI) · Ticket angelegt · –
2026-11-03 · DAVIDCOPCOP (KI) · Aufnahme/Export, Log-Tab, ADR-Nachtrag · –
