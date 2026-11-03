---
id: SIM-18
phase: "1b"
titel: Simulationsseite /sim/ maximal schlicht: nur 3D-Feld, Live-Statuszeile und Live-Knopf, weißer Hintergrund
status: review
owner: arjanssen
branch: ticket/sim-18
depends: [SIM-16]
hw: nein
---

## Ziel
Die Seite /sim/ (data-ansicht="sim") so einfach wie möglich machen. Vom Menschen per Screenshot markiert (3.11.), alles davon kommt auf /sim/ weg:
- Feldinfo oben links („Feld 144″ · 17 Feldpunkte gezeigt · 4 nicht vermessen …“, #fieldinfo)
- in der Live-Infobox (#liveinfo) die Tastenhilfe („W/S fahren · A/D drehen · …“) und die Hinweise START_STACKER/START_RUNNER zur Ersatzpose; die erste Zeile („Live · Stacker aktiv · v · x y h“) bleibt
- die ganze Zeitleiste unter dem Feld (.time: ▶, Zeitanzeige, Regler mit Ereignisstreifen, Geschwindigkeit)
- die ganze rechte Spalte mit dem Log (Titel, Zähler, Knöpfe #livectl, Filter-Chips, Ereignisliste) – Entscheidung des Menschen: Log auf /sim/ komplett entfernen
Dazu: Seiten- und Feldhintergrund schlicht weiß, keine grauen Flächen, Rahmen oder Schatten um das Fenster.

## Fertig, wenn …
- /sim/ zeigt nur noch: 3D-Feld mit Robotern über die volle Breite, die eine Live-Statuszeile und den Knopf „Live“
- Hintergrund von body und 3D-Szene (renderer clear color / --viewer) auf /sim/ #FFFFFF, auch wenn das System dunkles Design nutzt; Feldlinien und Roboter bleiben gut sichtbar
- Ohne Zeitleiste und Log startet /sim/ direkt im Live-Modus (keine Demo-Wiedergabe ohne Bedienelemente); Live-Knopf schaltet weiter ein/aus
- Nur Ausblenden per CSS/Modus-Schalter in sim-Ansicht, keine Logik löschen: Ereignisse werden intern weiter erfasst; /viewer/ sieht aus und funktioniert genau wie vorher (Zeitleiste, Ereignisse, Log laden, Feldinfo)
- tools/test_sim_page.py bzw. ein Browser-Test (Playwright wie bei SIM-13, falls vorhanden) prüft: auf /sim/ sind #fieldinfo, .time und die Log-Spalte nicht sichtbar, #liveinfo hat genau eine Zeile, Hintergrund weiß; auf /viewer/ sind sie sichtbar
- Laptop-Tests grün, beide Roboter bauen, `python tools/board.py validate` grün

## Kontext
Vom Menschen gewünscht (3.11.): „alles rot Durchgestrichene weg, Hintergrund normal weiß, so simpel wie möglich“. Baut auf dem Stand von SIM-16 auf (PR #45, gleiche Datei tools/viewer/index.html) – erst nach dessen Merge beginnen. Folge: CSV speichern/Log laden aus SIM-16 sind auf /sim/ nicht mehr erreichbar, im Viewer weiterhin. Regeln: CLAUDE.md.

## Hardware-Prüfung
—

## Übergabe
Advisor: JS-Syntax per JavaScriptCore geprüft (ok), Boden-Textur ist JS-fest, Autostart-Fehler nur console.warn. Stand: umgesetzt (nur CSS-Ausblenden in tools/viewer/index.html unter body[data-ansicht="sim"]; const SIM; autoLive startet Live nach dem Feld; Demo-Logs entfallen auf /sim/; liveinfo = eine Zeile). Tests: tools/test_sim_page.py (statische Prüfung, kein Playwright im Repo). Am echten Browser prüfen: weißer Hintergrund auch im Dark Mode, Feldlinien gut sichtbar, /viewer/ unverändert.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-03 · DAVIDCOPCOP (KI) · Ticket angelegt (nach Screenshot mit Markierungen) · –
2026-11-03 · arjanssen (KI) · /sim/ schlicht: Zeitleiste, Log, Feldinfo ausgeblendet, weiß, Autostart Live · –
