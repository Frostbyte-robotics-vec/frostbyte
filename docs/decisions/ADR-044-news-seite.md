# ADR-044: News-Seite aus Textdateien, gebaut vom Pages-Workflow

Status: angenommen (9. Nov 2026), Ticket W-06.

## Entscheidung
- Beiträge liegen als Textdateien in `news/beitraege/` (Kopfzeilen + einfacher Text), Bilder in `news/bilder/`. `tools/news_build.py` (nur Standardbibliothek) erzeugt daraus `site/news/` im Design der Hauptseite; `dashboard.yml` ruft es vor dem Deploy auf. Ein neuer Beitrag = eine Datei + Push; kein manuelles Hochladen.
- ADR-036 sagte „kein Build-Skript“, weil die Seite nur statisch war. Eine Liste, die wächst, von Hand in HTML zu pflegen (Index, Datum, Sortierung) ist fehleranfällig; deshalb dieses eine kleine Skript. Hauptseite und Sponsoringseite bleiben Handarbeit.
- Eigene Markdown-Teilmenge statt Paket (Absätze, `##`, Listen, fett, http(s)-Links): kein neues Paket nötig, aller Text wird HTML-maskiert, Links nur mit http/https.
- Entwürfe (`entwurf: ja`) werden nicht gebaut. Fehler (Datum, Kategorie, fehlendes Bild/Alt-Text) brechen den Bau mit Dateinamen ab, damit nichts Kaputtes live geht.
- Keine Beispielbeiträge: leere Liste zeigt einen ehrlichen Hinweis.

## Offen (Mensch)
- Eigene Domain: bei GitHub Pages in den Repo-Einstellungen eintragen und beim Domain-Anbieter einen CNAME-Eintrag setzen; Neuveröffentlichung der Inhalte läuft danach unverändert per Push.
- Bildrechte und Freigaben (z. B. Sponsoren nennen) klärt das Team je Beitrag.
