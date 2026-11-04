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
