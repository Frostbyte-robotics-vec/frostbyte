# ADR-050: Partnerstufen mit Logos und zwei Formulare auf der Teamseite

Status: angenommen (12. Nov 2026), Ticket W-14. Entscheidung des Menschen im Chat; Rat des Advisors (Opus) hat Formular und Teststrategie geprägt.

## Entscheidung
- **Stufen:** Auf der Partnerwand stehen zwei Bezeichnungen: „Hauptpartner“ (Novagear, großes Logo) und „Supporter“ (TechLabs, kleineres Logo); EN „Main partner“ / „Supporter“. Das ändert die W-09-Vorgabe „flache Wand, keine Stufen“ ausdrücklich (Mensch, 12. Nov 2026). Weiter nicht öffentlich: Pakete, Preise, Budget. norelem ist vorerst nicht auf der Seite (Mensch).
- **Tests sind angepasst, nicht abgeschwächt:** „Hauptpartner“/„Supporter“ sind als Wörter erlaubt, „TechLabs“ ist bestätigt, der norelem-Test wurde durch einen Test der neuen Struktur ersetzt (Logo, Stufe, Bezeichnung, beide Sprachen). Die Klassen `tierlbl`, `slot-main`, `grid3`, `grid6` bleiben verboten, das neue Markup nutzt `.partners`, `.ptier`, `.plogo`.
- **Logos:** `web/assets/sponsoren/novagear.png` und `techlabs.png`, vom Menschen geliefert. Kleingeschrieben, weil der Test die Schreibweise „NOVAGEAR“ im HTML verbietet. Logos sind nicht verlinkt (keine erfundenen Adressen).
- **Zwei Formulare, ohne Server:** `#bewerben` (Bewerbung fürs Team: Name, E-Mail, Studiengang/Semester, Interessengebiet, Nachricht) und `#partner-werden` (Firma, Ansprechperson, E-Mail, Art der Unterstützung, Nachricht). Beide setzen mit JavaScript eine E-Mail an frostbyte.eth.robotics@gmail.com zusammen und öffnen das Mail-Programm (`mailto:`, per angeklicktem Link, kein `location`). Es gibt keinen Dienst, keinen Server, kein Fremdskript, keine Cookies, nichts wird gespeichert.
- **Ohne JavaScript** ist das Formular ausgeblendet und ein Hinweis mit der Adresse sichtbar. `mailto:` mit POST oder GET trägt nur `subject` und `body` zuverlässig; ein halbes Formular wäre irreführend.
- Texte (Feldnamen, Betreff, Hinweise) stehen in der Seite (Labels, `data-`-Attribute), nicht in `site.js` (ADR-046). Gleiche ids in DE und EN, Paritätstest bleibt.

## Folgen
- Die Mail verlässt die Seite nur, wenn Besucher sie in ihrem Mail-Programm abschicken. Auf Handys ohne eingerichtetes Mail-Programm passiert beim Absenden nichts außer dem Hinweis mit der Adresse.
- Datenschutztext (impressum.html, PR #108) muss das Formular nennen: Angaben gehen nur per E-Mail an das Team, nichts liegt auf der Seite.
- Ein Formulardienst (Formspree o. Ä.) wäre bequemer, bräuchte aber einen Eintrag in docs/decisions/, einen Datenschutztext und einen Fremdserver; bewusst nicht gewählt.
