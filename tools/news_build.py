#!/usr/bin/env python3
"""News-Seite bauen (W-06, ADR-044): news/beitraege/*.md -> <out>/index.html und eine Seite je Beitrag.

Aufruf: python3 tools/news_build.py news site/news
Nur Standardbibliothek. Format der Beiträge: news/LIESMICH.md.
"""
import datetime
import html
import re
import shutil
import sys
from pathlib import Path

KATEGORIEN = ("Sponsoren", "Events", "Team", "Roboter", "Saison")
MONATE = ("Januar", "Februar", "März", "April", "Mai", "Juni", "Juli", "August", "September", "Oktober",
          "November", "Dezember")
BILD = re.compile(r"^[\w-]+(\.[\w-]+)*\.(jpg|jpeg|png|webp)$", re.I)
KOPF = re.compile(r"^(\w+):\s*(.*)$")


class NewsFehler(Exception):
    pass


def parse(text, dateiname):
    """Kopfzeilen (bis zur ersten Leerzeile) und Text eines Beitrags prüfen; Fehler nennen den Dateinamen."""
    def fehler(msg):
        return NewsFehler(f"{dateiname}: {msg}")

    kopf_text, _, rest = text.replace("\r\n", "\n").strip("\n").partition("\n\n")
    kopf = {}
    for zeile in kopf_text.split("\n"):
        m = KOPF.match(zeile)
        if not m:
            raise fehler(f"Kopfzeile nicht lesbar: {zeile!r}")
        kopf[m.group(1)] = m.group(2).strip()
    for pflicht in ("titel", "datum", "kategorie"):
        if not kopf.get(pflicht):
            raise fehler(f"Kopfzeile '{pflicht}' fehlt")
    if not re.fullmatch(r"\d{4}-\d{2}-\d{2}", kopf["datum"]):
        raise fehler("datum muss JJJJ-MM-TT sein")
    try:
        datum = datetime.date.fromisoformat(kopf["datum"])
    except ValueError:
        raise fehler(f"datum ungültig: {kopf['datum']}")
    if kopf["kategorie"] not in KATEGORIEN:
        raise fehler(f"kategorie muss eine von {', '.join(KATEGORIEN)} sein")
    bild = kopf.get("bild", "")
    if bild:
        if not BILD.match(bild):
            raise fehler(f"bild ungültig: {bild}")
        if not kopf.get("bild_alt"):
            raise fehler("bild braucht bild_alt (Beschreibung)")
    return {"titel": kopf["titel"], "datum": kopf["datum"], "date": datum, "kategorie": kopf["kategorie"],
            "bild": bild, "bild_alt": kopf.get("bild_alt", ""), "entwurf": kopf.get("entwurf", "").lower() == "ja",
            "slug": Path(dateiname).stem, "text": rest.strip()}


def _inline(s):
    s = html.escape(s)

    def link(m):
        url = m.group(2)
        if url.startswith(("http://", "https://")):
            return f'<a href="{url}" rel="noopener">{m.group(1)}</a>'
        return m.group(1)

    s = re.sub(r"\[([^\]]+)\]\(([^)\s]+)\)", link, s)
    return re.sub(r"\*\*(.+?)\*\*", r"<strong>\1</strong>", s)


def render_text(text):
    """Teilmenge von Markdown: Absätze, '## ', '- '-Listen, **fett**, [Text](http-URL). Alles andere wird maskiert."""
    teile = []
    for block in re.split(r"\n\s*\n", text.strip()):
        zeilen = [z.rstrip() for z in block.split("\n") if z.strip()]
        if not zeilen:
            continue
        if zeilen[0].startswith("## "):
            teile.append(f"<h2>{_inline(zeilen[0][3:])}</h2>")
            zeilen = zeilen[1:]
            if not zeilen:
                continue
        if all(z.startswith("- ") for z in zeilen):
            teile.append("<ul>" + "".join(f"<li>{_inline(z[2:])}</li>" for z in zeilen) + "</ul>")
        else:
            teile.append(f"<p>{_inline(' '.join(z.strip() for z in zeilen))}</p>")
    return "\n".join(teile)


def _datum_de(d):
    return f"{d.day}. {MONATE[d.month - 1]} {d.year}"


def _auszug(text, n=180):
    erster = re.split(r"\n\s*\n", text.strip())[0] if text.strip() else ""
    klar = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", erster).replace("**", "").replace("## ", "")
    klar = " ".join(klar.split())
    return klar if len(klar) <= n else klar[:n].rsplit(" ", 1)[0] + " …"


def seite(titel, beschreibung, inhalt):
    return f"""<!doctype html>
<html lang="de">
<head>
<title>{html.escape(titel)} – Frostbyte Robotics</title>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="description" content="{html.escape(beschreibung, quote=True)}">
<meta property="og:title" content="{html.escape(titel, quote=True)}">
<meta property="og:description" content="{html.escape(beschreibung, quote=True)}">
<meta property="og:image" content="../assets/snowflake.png">
<meta name="theme-color" content="#071a33">
<link rel="icon" type="image/png" href="../assets/favicon.png">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Barlow+Condensed:wght@500;600;700;800&family=Barlow:wght@400;500;600&family=Chakra+Petch:wght@600;700&family=JetBrains+Mono:wght@500;700&display=swap">
<link rel="stylesheet" href="../assets/style.css">
</head>
<body>
<nav class="nav dark" aria-label="Hauptnavigation">
  <div class="wrap">
    <a class="mark" href="../index.html" aria-label="Frostbyte – zur Hauptseite"><img src="../assets/snowflake.png" alt=""><span>FROST<span class="b">BYTE</span><span class="cur" aria-hidden="true"></span></span></a>
    <ul id="menu">
      <li><a href="../index.html#team">Team</a></li>
      <li><a href="../index.html#saison">Saison</a></li>
      <li><a href="index.html">News</a></li>
    </ul>
    <div class="nav-cta">
      <a class="btn sm ghost" href="../sponsoring.html">Unterstützen <span class="arr">→</span></a>
      <button class="burger" id="burger" aria-label="Menü öffnen" aria-expanded="false" aria-controls="menu"><span></span><span></span><span></span></button>
    </div>
  </div>
  <div class="progress" id="progress"></div>
</nav>
{inhalt}
<footer class="dark">
  <div class="wrap">
    <div class="f-top">
      <div>
        <a class="mark" href="../index.html"><img src="../assets/snowflake.png" alt="">FROST<span class="b">BYTE</span><span class="cur" aria-hidden="true"></span></a>
        <p style="margin-top:14px;max-width:34ch">VEX-U-Studierendenteam der ETH Zürich.</p>
      </div>
      <div><h4>Seiten</h4><ul><li><a href="../index.html">Hauptseite</a></li><li><a href="index.html">News</a></li><li><a href="../sponsoring.html">Unterstützen</a></li></ul></div>
    </div>
    <div class="f-bot"><span>© 2026 Frostbyte · ETH Zürich</span><span class="bin" aria-hidden="true">01000110 01010010 01001111 01010011 01010100</span><span>frostbyte.eth.robotics@gmail.com</span></div>
  </div>
</footer>
<div class="toast" id="toast" role="status" aria-live="polite"></div>
<script src="../assets/site.js"></script>
</body>
</html>
"""


def _index(beitraege):
    if beitraege:
        karten = "\n".join(
            f"""      <article class="news-card">
        <div class="news-meta"><time datetime="{p['datum']}">{_datum_de(p['date'])}</time><span class="news-tag">{p['kategorie']}</span></div>
        <h2><a href="{p['slug']}.html">{html.escape(p['titel'])}</a></h2>
        <p>{html.escape(_auszug(p['text']))}</p>
      </article>""" for p in beitraege)
    else:
        karten = '      <p class="news-leer">Noch keine Beiträge. Hier erscheinen bald Neuigkeiten von Frostbyte.</p>'
    inhalt = f"""
<header class="page-head dark">
  <div class="wrap">
    <div class="eyebrow">News · Saison 2026/27</div>
    <h1><span class="line"><span>Neuigkeiten von <span class="hl">Frostbyte.</span></span></span></h1>
    <p class="lead">Neue Partner, Events und Fortschritte am Roboter.</p>
  </div>
</header>
<section class="sec">
  <div class="wrap">
    <div class="news-list">
{karten}
    </div>
  </div>
</section>"""
    return seite("News", "Neuigkeiten von Frostbyte, dem VEX-U-Team der ETH Zürich: Partner, Events und Fortschritte.", inhalt)


def _beitrag(p):
    bild = (f'\n      <figure class="news-bild"><img src="bilder/{p["bild"]}" alt="{html.escape(p["bild_alt"], quote=True)}"></figure>'
            if p["bild"] else "")
    inhalt = f"""
<header class="page-head dark">
  <div class="wrap">
    <div class="eyebrow">{p['kategorie']} · <time datetime="{p['datum']}">{_datum_de(p['date'])}</time></div>
    <h1><span class="line"><span>{html.escape(p['titel'])}</span></span></h1>
    <a class="back" href="index.html">← alle News</a>
  </div>
</header>
<section class="sec">
  <div class="wrap">
    <div class="news-text">{bild}
{render_text(p['text'])}
    </div>
  </div>
</section>"""
    return seite(p["titel"], _auszug(p["text"]) or p["titel"], inhalt)


def build(src, out):
    src, out = Path(src), Path(out)
    beitraege = []
    for f in sorted((src / "beitraege").glob("*.md")) if (src / "beitraege").is_dir() else []:
        p = parse(f.read_text(encoding="utf-8"), f.name)
        if p["bild"] and not (src / "bilder" / p["bild"]).is_file():
            raise NewsFehler(f"{f.name}: Bild fehlt in news/bilder/: {p['bild']}")
        if not p["entwurf"]:
            beitraege.append(p)
    beitraege.sort(key=lambda p: (p["datum"], p["slug"]), reverse=True)
    out.mkdir(parents=True, exist_ok=True)
    (out / "index.html").write_text(_index(beitraege), encoding="utf-8")
    for p in beitraege:
        (out / f"{p['slug']}.html").write_text(_beitrag(p), encoding="utf-8")
        if p["bild"]:
            (out / "bilder").mkdir(exist_ok=True)
            shutil.copy2(src / "bilder" / p["bild"], out / "bilder" / p["bild"])
    return len(beitraege)


def main(argv):
    if len(argv) != 3:
        print("Aufruf: news_build.py <news-Ordner> <Ausgabeordner>", file=sys.stderr)
        return 2
    try:
        n = build(argv[1], argv[2])
    except NewsFehler as e:
        print(f"FEHLER {e}", file=sys.stderr)
        return 1
    print(f"News: {n} Beiträge -> {argv[2]}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
