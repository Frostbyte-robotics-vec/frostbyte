---
id: SIM-47
phase: "5"
titel: Viewer auf der Webseite: Lauf wählbar (Skills-Routen oder Demo), Teampunkte sichtbar
status: done
owner: arjanssen
branch: ticket/sim-47
depends: []
hw: nein
---

## Ziel
Der Mensch (2026-11-15) sieht unter /viewer/ nur den Demo-Lauf (`frostbyte_sim --demo`): beide Roboter fahren geradeaus, drehen sich zueinander, rammen sich, keine Punkte. Der Workflow `dashboard.yml` erzeugt nur diesen Demo-Lauf, die Skills-Routen aus `routes/` kommen auf der Seite nicht vor, deshalb sind alle Messungen der Sim-Tickets dort nicht sichtbar.
- `dashboard.yml` lässt zusätzlich die Skills-Routen laufen (`frostbyte_sim --stacker routes/skills_stacker.route --runner routes/skills_runner.route --seed 1`) und legt die Logs unter `site/viewer/lauf/skills/` ab.
- Der Viewer bekommt eine Auswahl „Lauf“ (Skills-Routen / Demo), gespeist aus einer Liste `tools/viewer/laeufe.json`; Standard = Skills-Routen. Wechsel ersetzt die Logs, springt auf t = 0.
- Teampunkte (Teamsumme aus der `score`-Spalte, ADR-026) sind neben der Zeitleiste gut sichtbar, nicht nur in der Tabelle.

## Fertig, wenn …
- Test (Python): `laeufe.json` ist gültig, jeder Eintrag hat Anzeigename und Ordner, und `dashboard.yml` erzeugt für jeden Ordner Logs (`--out site/viewer/<ordner>`) mit vorhandenen Routen-Dateien
- Im Browser (lokal mit `python3 -m http.server`, Headless-Chrome-Screenshot): Auswahl zeigt beide Läufe; beim Skills-Lauf steigen die Teampunkte im Lauf der Zeit (Wert bei t = 60 s im Ticket), der Demo-Lauf bleibt wählbar
- Seite `/sim/` (Live-Modus) unverändert; Laptop-Tests, Python-Tests, `board.py validate` grün; clang-format ok
- Nach dem Merge: auf der Veröffentlichung gegenprüfen (Mensch)

## Kontext
.github/workflows/dashboard.yml, tools/viewer/index.html, tools/sim_page.py, ADR-025/026/049, SIM-44, SIM-46.

## Hardware-Prüfung
—

## Übergabe
(aktueller Stand für die nächste Sitzung – wird bei jedem Sitzungsende überschrieben)
Stand 2026-11-15 (fertig, PR offen): `tools/viewer/laeufe.json` (Skills-Routen als Standard, Demo), Auswahl „Lauf“ und Anzeige „Punkte“ (Teamsumme) über der Zeitleiste in `tools/viewer/index.html` (`?lauf=<id>` wählt einen Lauf), `dashboard.yml` erzeugt `site/viewer/lauf/skills/` aus den Skills-Routen (Seed 1) und kopiert `laeufe.json`. Test `tools/viewer/test_runs.py` (4 Tests, zuerst rot). Im Browser geprüft (lokaler Server, Headless-Chrome): Skills-Lauf bei t = 55 s: 56 Punkte (Stand main ohne #125/#127); Demo bei t = 30 s: 20 Punkte. Ohne `laeufe.json` (CI-Artefakt viewer-demo, lokal) fällt der Viewer auf die Demo zurück. Der Skills-Lauf zeigt nach dem Merge von #125/#127 automatisch die neuen Werte (Fusion, Anfahrposen). Nach dem Merge auf der Veröffentlichung gegenprüfen (Mensch). Grenzen: nur Seed 1, kein Seed-Wähler; Viewer-JS-Tests (node) hier nicht gelaufen.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-15 · arjanssen (KI) · Ticket angelegt (Wunsch des Menschen) · –
2026-11-15 · arjanssen (KI) · Lauf-Auswahl, Teampunkte, Workflow, Test, Browser-Prüfung · –
