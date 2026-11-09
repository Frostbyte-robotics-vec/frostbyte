# News-Beiträge schreiben

Ein Beitrag ist eine Textdatei in `news/beitraege/`. Name: `JJJJ-MM-TT-kurzer-name.md`, z. B. `2026-12-05-event-zuerich.md`.
Nach dem Push auf `main` veröffentlicht der Pages-Workflow die Seite automatisch (kein manuelles Hochladen).

```
titel: Wir waren beim Event in Zürich
datum: 2026-12-05
kategorie: Events
bild: team-stand.jpg
bild_alt: Das Team am Stand
entwurf: ja

Erster Absatz. **Fett** und [Link](https://beispiel.ch).

## Zwischenüberschrift
- Liste
- Liste
```

- Pflicht: `titel`, `datum` (JJJJ-MM-TT), `kategorie` (Sponsoren, Events, Team, Roboter, Saison). Danach eine Leerzeile, dann der Text.
- Bild (optional): Datei nach `news/bilder/` legen, `bild:` und `bild_alt:` (Beschreibung für Sehbehinderte) angeben. Nur Bilder, die wir verwenden dürfen.
- `entwurf: ja` = wird nicht veröffentlicht. Zum Veröffentlichen die Zeile löschen.
- Links nur mit `http(s)://`. Anderes HTML wird nicht übernommen.
- Lokal ansehen: `mkdir -p site && cp -r web/. site/ && python3 tools/news_build.py news site/news`, dann `site/news/index.html` im Browser öffnen (`site/` ist nicht im Repo).
