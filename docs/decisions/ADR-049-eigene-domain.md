# ADR-049: Eigene Domain frostbyterobotics.ch für die Teamseite

Status: angenommen (11. Nov 2026), Ticket W-10. Domains vom Menschen bei GoDaddy gekauft und eingerichtet.

## Entscheidung
- Hauptadresse ist https://frostbyterobotics.ch (GitHub Pages, HTTPS erzwungen). `frostbyterobotics.com` ist nur eine Weiterleitung auf die .ch, inzwischen über Cloudflare (Stand 11. Nov 2026, vom Menschen getestet: funktioniert, auch per https).
- DNS der .ch bei GoDaddy: vier A-Einträge für `@` (185.199.108.153, 185.199.109.153, 185.199.110.153, 185.199.111.153), CNAME `www` → frostbyte-robotics-vec.github.io. NS, SOA, `_domainconnect` und `_dmarc` sind GoDaddy-Standard und bleiben.
- GitHub: Settings → Pages → Custom domain = frostbyterobotics.ch, Enforce HTTPS an. Es gibt bewusst keine CNAME-Datei im Repo: der Actions-Workflow (`dashboard.yml`) veröffentlicht per Pages-Artefakt, und die Einstellung in Settings genügt. Eine Datei wäre wirkungslos und würde nur zwei Wahrheiten erzeugen.
- Links in `web/` sind relativ oder zeigen auf https://frostbyterobotics.ch; `tools/test_web.py` lehnt `github.io` und Pfade mit `/frostbyte/` ab. Kanonische Adressen und hreflang: ADR-046.
- Entscheidung des Menschen (11. Nov 2026): Unter der Domain soll nur die Teamseite öffentlich sein. `/board/`, `/viewer/` und `/sim/` werden aus dem Pages-Build genommen (Ticket W-12). Bis dahin sind sie noch erreichbar.

## Offen (Mensch)
- Domain bei GitHub verifizieren (Organisation → Settings → Pages → Add a domain, TXT-Eintrag bei GoDaddy, bleibt dauerhaft). Schützt vor Übernahme der Domain durch andere Konten.
- Impressum/Datenschutz: die Seite hat keines (kein Treffer in `web/`). Der Mensch liefert die Angaben (verantwortliche Person, Kontakt, Datenhinweis); bis dahin keine Seite (ADR-035: keine Platzhaltertexte). Umsetzung: Ticket W-13, blocked bis die Angaben da sind.
