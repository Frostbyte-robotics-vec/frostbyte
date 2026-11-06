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
- **Livestream-Zahl:** Der Mensch bestätigte (2026-11-06) „rund 160'000 Menschen haben den Livestream des letzten Weltfinales angesehen“; Text im Abschnitt Reichweite. Eine Quelle dafür liegt nicht im Repo (Websuche fand keine); wer sie hat, trägt sie hier ein.
- **Kontakt:** frostbyte.eth.robotics@gmail.com (Hauptseite und Sponsoringseite, Kopier-Knopf, Fusszeile); kein Personenname mehr im Kontaktblock. Fusszeile ohne Zielsatz.
- **Instagram/LinkedIn:** vom Menschen genannt: instagram.com/frostbyte.robotics und linkedin.com/company/frostbyterobotics; auf der Seite verlinkt (Instagram-Adresse aus dem Namen abgeleitet, Link nicht geprüft).
- **Offen (Mensch):**
  1. Bilder (teilweise erledigt): Auf Wunsch des Menschen (Variante „gezielt ergänzen“) ersetzen zwei frei lizenzierte Fotos von Wikimedia Commons die unscharfen Vollbilder: `worlds-match.jpg` (Rick Tyler, CC BY-SA 3.0, Reichweite-Block) und `team-trophy.jpg` (Wrc Ynapmoc, CC BY 3.0, Saison-Ziel). Bildnachweis steht in der Fusszeile der Hauptseite (Pflicht bei CC BY). Lizenz laut Commons-API am 2026-11-06 gelesen. Weiter unscharf und unverändert: `arena.jpg`, `champions.jpg`, `vex-stage.jpg` im Mosaik „Die Liga“ (nur kleine Kacheln); ihre Herkunft und Rechte sind nicht dokumentiert. Früherer Stand: `arena.jpg`, `champions.jpg`, `vex-stage.jpg` sind nur 990 px breit und wirken als Vollbild unscharf. Der Mensch erlaubte Bilder aus der Google-Suche; die KI hat keine übernommen (Rechte liegen bei Fotografen/VEX, öffentlich gehostet nicht ohne Weiteres zulässig). Ersatz: eigene Fotos des Teams oder Bilder mit Freigabe/Lizenz (z. B. Wikimedia Commons), Originale in `web/assets/` ablegen.

## Nachtrag W-04 (6. Nov 2026): Mosaik „Die Liga“ mit freien Fotos
Auf Wunsch des Menschen ersetzen vier scharfe, frei lizenzierte Fotos von Wikimedia Commons die unscharfen Kacheln (`liga-1..4.jpg`, 1300 px): drei gemeinfreie Fotos von Sgt. 1st Class Lisa Litchfield (US Army, VEX-Turnier 2014: „One last adjustment“, „Fine-tuning Stubmeister the Stubbier“, „Judgment day at Sunset Station“) und „VEX Starstruck Standard Fields“ von Sean Spicer (CC BY-SA 4.0). Quellen und Lizenzen stehen im Bildnachweis der Fusszeile (Lizenz laut Commons-API am 2026-11-06 gelesen). `event-field.jpg`, `vex-stage.jpg`, `champions.jpg` und `arena.jpg` (Herkunft und Rechte undokumentiert) sind aus `web/assets/` entfernt; `tools/test_web.py` stellt sicher, dass sie nicht zurückkehren. Damit ist der Bilderpunkt aus dem Nachtrag W-03 erledigt. Die Fotos sind von 2014 und zeigen nicht das Weltfinale; die Alt-Texte behaupten das nicht.

**Änderung (6. Nov 2026, Entscheidung des Menschen):** Der Mensch wollte mindestens ein Arena-Foto mit grossem Publikum. Eine Suche (Commons, DVIDS, Flickr) fand kein frei lizenziertes. Auf ausdrücklichen Wunsch bleibt `arena.jpg` (aus der gelieferten Seitenvorlage, 990 px, Herkunft und Rechte nicht dokumentiert) als eine kleine Kachel „Weltfinale“ im Mosaik, ersetzt `liga-3.jpg` (entfernt). Rechte sind **nicht geklärt**; das Risiko trägt das Team. Sauberer Ersatz: Foto mit Freigabe der REC Foundation/VEX oder eigenes Foto, dann `arena.jpg` löschen. Die übrigen drei Kacheln bleiben frei lizenziert.
