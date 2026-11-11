# ADR-046: Webseite zweisprachig als getrennte statische Seiten

Status: angenommen (11. Nov 2026), Ticket W-11. Rat des Advisors (Opus) hat die Entscheidung geprägt.

## Entscheidung
- Deutsch bleibt `web/index.html` (Adresse `/`), Englisch ist `web/en/index.html` (Adresse `/en/`). Beide sind fertiges HTML; der Pages-Workflow kopiert `web/` unverändert (`cp -r web/. site/`).
- Kein JavaScript-Wörterbuch mit Umschalter: Besucher ohne JS und Suchmaschinen sähen nur eine Sprache, und die Tests könnten den Text nicht prüfen.
- Umschalter ist ein normaler Link (`class="lang"`) in Navigation und Fußzeile. Keine automatische Weiterleitung nach Browsersprache (Test verbietet `navigator.language`, `location.*`, `http-equiv=refresh`).
- Beide Seiten haben `lang`, `rel="canonical"` und `hreflang` (de, en, x-default) mit der Adresse https://frostbyterobotics.ch (Domain: W-10).
- Die `id`s bleiben in beiden Sprachen gleich (deutsch, z. B. `#saison`), damit Anker und Paritätstest einfach sind.
- News bleibt deutsch (ADR-044); die englische Navigation verweist mit „News (DE)“ darauf.
- Ein Paritätstest in `tools/test_web.py` prüft: gleiche ids, gleiche Abschnittsreihenfolge, gleiche Bilder, gleiche Zähler. Wer die eine Seite ändert, muss die andere nachziehen, sonst wird die CI rot.

## Folgen
- Jeder neue Abschnitt wird zweimal geschrieben. Das ist gewollt: Übersetzung ist Redaktion, kein Automatismus.
- Texte, die im Skript stehen (Toast beim Kopieren), kommen aus `data-`-Attributen der Seite, nicht aus `site.js`.
