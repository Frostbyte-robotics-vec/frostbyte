# ADR-035: Öffentliche Teamseite an der Pages-Wurzel, Board unter /board/

Status: angenommen (4. Nov 2026), Ticket W-01.

## Entscheidung
- Die öffentliche Seite (Sponsoren, Hauptsponsor, Team) liegt in `web/` und wird von `tools/web_build.py` nach `site/index.html` gebaut. Veröffentlicht wird sie im bestehenden Pages-Workflow `dashboard.yml`.
- Die interne Fortschrittsseite wandert nach `<Pages-URL>/board/`. `/viewer/` und `/sim/` bleiben, die Verweise im Board-Template zeigen auf `../viewer/` usw.
- Inhalte, die sich ändern (Sponsoren, Team, Kontakt, Roboter), stehen in `web/inhalt.json`, nicht im HTML. Eine neue Sponsorenzeile plus Logo in `web/img/sponsoren/` genügt. `web_build.py` prüft die Datei und bricht bei Fehlern ab.
- Kein Framework, kein JavaScript, keine Web-Schriften, kein Build-Werkzeug außer einem Python-Skript mit Standardbibliothek. Grund: Die Seite hat eine Handvoll statischer Abschnitte; jede Abhängigkeit wäre mehr Wartung als der Nutzen. Die Gestaltungsvorgaben (weiß, Standardfarben, keine Animation) sind mit reinem CSS trivial.
- Leere Listen blenden ihren Abschnitt aus. Es gibt keine Beispiel-Sponsoren und keine Platzhaltertexte auf der veröffentlichten Seite.

## Nicht entschieden (Mensch)
Sprache, eigene Domain, eigenes Repo für eine Adresse ohne `/frostbyte/`, Texte und Kontaktdaten: siehe tasks/W-01.md.
