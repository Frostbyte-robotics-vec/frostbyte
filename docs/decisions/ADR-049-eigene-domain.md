# ADR-049: Eigene Domain frostbyterobotics.ch für die Teamseite

Status: angenommen (11. Nov 2026), Ticket W-10. Domains vom Menschen bei GoDaddy gekauft und eingerichtet.

## Entscheidung
- Hauptadresse ist https://frostbyterobotics.ch (GitHub Pages, HTTPS erzwungen). `frostbyterobotics.com` ist nur eine GoDaddy-Weiterleitung auf die .ch.
- DNS der .ch bei GoDaddy: vier A-Einträge für `@` (185.199.108.153, 185.199.109.153, 185.199.110.153, 185.199.111.153), CNAME `www` → frostbyte-robotics-vec.github.io. NS, SOA, `_domainconnect` und `_dmarc` sind GoDaddy-Standard und bleiben.
- GitHub: Settings → Pages → Custom domain = frostbyterobotics.ch, Enforce HTTPS an. Es gibt bewusst keine CNAME-Datei im Repo: der Actions-Workflow (`dashboard.yml`) veröffentlicht per Pages-Artefakt, und die Einstellung in Settings genügt. Eine Datei wäre wirkungslos und würde nur zwei Wahrheiten erzeugen.
- Links in `web/` sind relativ oder zeigen auf https://frostbyterobotics.ch; `tools/test_web.py` lehnt `github.io` und Pfade mit `/frostbyte/` ab. Kanonische Adressen und hreflang: ADR-046.
- `/board/`, `/viewer/` und `/sim/` bleiben unter der öffentlichen Domain erreichbar (Stand heute alle öffentlich, wie das Repo). Änderung nur nach Entscheidung des Menschen.

## Offen (Mensch)
- Domain bei GitHub verifizieren (Organisation → Settings → Pages → Add a domain, TXT-Eintrag bei GoDaddy, bleibt dauerhaft). Schützt vor Übernahme der Domain durch andere Konten.
- `https://frostbyterobotics.com` (https) lief am 11. Nov in einen Zertifikats-Timeout, GoDaddy stellt für Weiterleitungen kein eigenes Zertifikat aus. Nach ein bis zwei Stunden erneut testen; sonst Cloudflare als Weiterleitung (gratis).
- Impressum/Datenschutz: die Seite hat keines (kein Treffer in `web/`). Verantwortliche Person, Kontakt und Hinweis zu erhobenen Daten muss der Mensch liefern; ohne Inhalt keine Seite (ADR-035: keine Platzhaltertexte auf der veröffentlichten Seite).
