# ADR-036: Öffentliche Seite ist die statische Sponsoringseite des Teams (ersetzt ADR-035)

Status: angenommen (4. Nov 2026), Ticket W-02. Ersetzt die Gestaltungs- und Bauentscheidung aus ADR-035; Ort (Pages-Wurzel, Board unter `/board/`) bleibt.

## Entscheidung
- Der Mensch hat eine fertige Seite geliefert (dunkle Eis-Nacht-Optik, Animationen, Google Fonts, kleines JavaScript, Sponsoringpakete, Budget, Team mit Fotos). Sie ersetzt die schlichte weiße Seite aus W-01. Die Vorgaben „keine Animation, keine Web-Fonts, kein JavaScript“ aus tasks/W-01.md gelten damit nicht mehr; sie stammten vom selben Menschen und sind überholt.
- Die Seite liegt unverändert als Handarbeit in `web/index.html` mit `web/assets/`. Kein Build-Skript: der Workflow `dashboard.yml` kopiert `web/` nach `site/`. `tools/web_build.py` und `web/inhalt.json` entfallen. Sponsor eintragen = Platz in `#partner` im HTML ersetzen (Kommentar dort).
- `tools/test_web.py` prüft nur Verweise: lokale Dateien vorhanden, Anker haben Ziele, Bilder haben `alt`.

## Folge / offen (Mensch)
- Schriften kommen von fonts.googleapis.com: Besucher-IP geht an Google. Für Schweiz/EU ist Selbsthosten (woff2 in `web/assets/`) sauberer; eigenes Ticket, falls gewünscht.
- Kein Impressum/Datenschutzhinweis auf der Seite; Kontakt nennt eine ETH-Adresse. Nutzung von ETH-Namen und Professorenfoto: Freigabe klären.
- Zahlen (Teamzahlen am Weltfinale, Budget, Paketpreise) stammen aus der Unterlage, nicht aus dem Repo geprüft.
- Instagram/LinkedIn/YouTube stehen auf `#`.

## Nachtrag W-03 (6. Nov 2026): Seite für alle, Sponsoring auf eigener Seite
Wunsch des Menschen: Die öffentliche Seite soll sich an alle richten, nicht nur an Sponsoren. Das Design bleibt.
- **Sponsoring** (Budget, Pakete, Partner-Wand, Sponsoring-Kontakt) liegt jetzt in `web/sponsoring.html`; die Hauptseite verweist über „Unterstützen“ (Navigation), einen Knopf im Kontaktabschnitt und die Fusszeile darauf. Hauptseite hat keine Sponsoring-Abschnitte und kein „Partner werden“ im Hero.
- **Gemeinsame Dateien:** CSS in `web/assets/style.css`, Skript in `web/assets/site.js` (jede Funktion prüft, ob ihr Element existiert). Weiterhin kein Build-Skript.
- **Laufband** ohne Werkzeugnamen: Zwei Roboter · Ein Team · Gebaut in Zürich · Erste Saison · Override 2026/27 · Ziel Las Vegas · Top 16. Werkzeug-Buttons im Prozess-Abschnitt entfernt, Prozesstexte ohne Softwarenamen. Saison-Zeitleiste mit Phasenmarken, „Wir sind hier“ und hervorgehobenem Ziel.
- **Social Media:** nur Instagram und LinkedIn (kein YouTube); Links stehen weiter auf `#`.
- **Offen (Mensch):**
  1. Livestream-Zahl des Vorjahres (genannt: etwa 160'000): ohne Quelle nicht auf der Seite; Platz und Hinweis als HTML-Kommentar im Abschnitt Reichweite. Eine Websuche fand keine belegte Zahl.
  2. Bilder: `arena.jpg`, `champions.jpg`, `vex-stage.jpg` sind nur 990 px breit und wirken als Vollbild unscharf. Ersatz nur mit geklärten Rechten (eigene Fotos des Teams, oder schriftliche Freigabe von REC Foundation/VEX); eine gefundene Quelle erlaubt ausdrücklich nichts. Bitte Originale ablegen, die KI bindet sie ein.
