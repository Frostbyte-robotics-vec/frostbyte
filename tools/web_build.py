"""Baut die öffentliche Teamseite (W-01): web/index.html + web/inhalt.json -> site/index.html.

Aufruf: python3 tools/web_build.py [web-Ordner] [Ausgabeordner]   (Standard: web site)
Reines HTML/CSS ohne JavaScript. Leere Listen blenden ihren Abschnitt aus; nichts wird erfunden.
"""
import html
import json
import shutil
import sys
from pathlib import Path

STUFEN = ("haupt", "gold", "silber", "unterstuetzer")
STUFEN_TITEL = {"gold": "Gold", "silber": "Silber", "unterstuetzer": "Unterstützer"}


def pruefe(inhalt, web):
    """Gibt eine Liste von Fehlertexten zurück (leer = gültig)."""
    f = []
    haupt = 0
    for i, s in enumerate(inhalt.get("sponsoren", [])):
        n = f"sponsoren[{i}]"
        for k in ("name", "stufe", "logo", "url"):
            if not s.get(k):
                f.append(f"{n}: Feld {k} fehlt")
        if s.get("stufe") and s["stufe"] not in STUFEN:
            f.append(f"{n}: stufe {s['stufe']!r} ungültig (erlaubt: {', '.join(STUFEN)})")
        if s.get("stufe") == "haupt":
            haupt += 1
        if s.get("url") and not s["url"].startswith("https://"):
            f.append(f"{n}: url muss mit https:// beginnen")
        logo = s.get("logo")
        if logo:
            if "/" in logo or "\\" in logo or logo.startswith("."):
                f.append(f"{n}: logo ist nur ein Dateiname in img/sponsoren/")
            elif not (Path(web) / "img" / "sponsoren" / logo).is_file():
                f.append(f"{n}: Logo-Datei img/sponsoren/{logo} fehlt")
    if haupt > 1:
        f.append("höchstens ein Hauptsponsor erlaubt")
    for i, m in enumerate(inhalt.get("team", [])):
        for k in ("name", "rolle"):
            if not m.get(k):
                f.append(f"team[{i}]: Feld {k} fehlt")
    for i, r in enumerate(inhalt.get("roboter", [])):
        if not r.get("name"):
            f.append(f"roboter[{i}]: Feld name fehlt")
        if r.get("bild") and not r.get("alt"):
            f.append(f"roboter[{i}]: Bild ohne alt-Text")
        if r.get("bild") and (("/" in r["bild"]) or r["bild"].startswith(".")):
            f.append(f"roboter[{i}]: bild ist nur ein Dateiname in img/")
        elif r.get("bild") and not (Path(web) / "img" / r["bild"]).is_file():
            f.append(f"roboter[{i}]: Bild img/{r['bild']} fehlt")
    return f


def e(text):
    return html.escape(str(text), quote=True)


def logo_link(s):
    img = f'<img src="img/sponsoren/{e(s["logo"])}" alt="Logo {e(s["name"])}">'
    return f'<a href="{e(s["url"])}">{img}</a>'


def abschnitt_haupt(inhalt):
    h = [s for s in inhalt.get("sponsoren", []) if s["stufe"] == "haupt"]
    if h:
        s = h[0]
        return (f'<section id="hauptsponsor" class="haupt">\n<h2>Hauptsponsor</h2>\n'
                f'<p>{logo_link(s)}</p>\n<p><a href="{e(s["url"])}">{e(s["name"])}</a></p>\n</section>\n')
    return ('<section id="hauptsponsor">\n<h2>Hauptsponsor werden</h2>\n'
            '<p>Der Platz des Hauptsponsors ist frei. Wir freuen uns über eine Anfrage, siehe <a href="#kontakt">Kontakt</a>.</p>\n'
            '</section>\n')


def abschnitt_roboter(inhalt):
    rs = inhalt.get("roboter", [])
    if not rs:
        return ""
    out = ['<section id="roboter">\n<h2>Roboter</h2>\n']
    for r in rs:
        out.append(f'<h3>{e(r["name"])}</h3>\n')
        if r.get("groesse_in"):
            out.append(f'<p>Größe: {e(r["groesse_in"])}″</p>\n')
        if r.get("bild"):
            out.append(f'<p><img src="img/{e(r["bild"])}" alt="{e(r["alt"])}"></p>\n')
        if r.get("text"):
            out.append(f'<p>{e(r["text"])}</p>\n')
    out.append("</section>\n")
    return "".join(out)


def abschnitt_team(inhalt):
    t = inhalt.get("team", [])
    if not t:
        return ""
    li = "".join(f'<li>{e(m["name"])}, {e(m["rolle"])}</li>\n' for m in t)
    return f'<section id="team">\n<h2>Team</h2>\n<ul>\n{li}</ul>\n</section>\n'


def abschnitt_sponsoren(inhalt):
    k = inhalt.get("kontakt", {})
    ss = [s for s in inhalt.get("sponsoren", []) if s["stufe"] != "haupt"]
    out = ['<section id="sponsoren" class="sponsoren">\n<h2>Sponsoren</h2>\n']
    for stufe in ("gold", "silber", "unterstuetzer"):
        gruppe = [s for s in ss if s["stufe"] == stufe]
        if gruppe:
            out.append(f"<h3>{STUFEN_TITEL[stufe]}</h3>\n<p>")
            out.append("\n".join(f'{logo_link(s)}<a href="{e(s["url"])}">{e(s["name"])}</a>' for s in gruppe))
            out.append("</p>\n")
    out.append("<h3>Sponsor werden</h3>\n")
    if inhalt.get("sponsor_werden"):
        out.append(f'<p>{e(inhalt["sponsor_werden"])}</p>\n')
    if k.get("email"):
        out.append(f'<p>Schreiben Sie uns: <a href="mailto:{e(k["email"])}">{e(k["email"])}</a></p>\n')
    else:
        out.append('<p>Sie möchten Frostbyte unterstützen? Siehe <a href="#kontakt">Kontakt</a>.</p>\n')
    out.append("</section>\n")
    return "".join(out)


def abschnitt_kontakt(inhalt):
    k = inhalt.get("kontakt", {})
    out = ['<section id="kontakt">\n<h2>Kontakt und Impressum</h2>\n']
    if k.get("email"):
        out.append(f'<p>E-Mail: <a href="mailto:{e(k["email"])}">{e(k["email"])}</a></p>\n')
    if k.get("verantwortlich"):
        out.append(f'<p>Verantwortlich: {e(k["verantwortlich"])}</p>\n')
    out.append("<p>Diese Seite erhebt keine Daten.</p>\n</section>\n")
    return "".join(out)


def nav(teile):
    links = [("#ueber-uns", "Über uns")]
    for sid, titel in (("roboter", "Roboter"), ("team", "Team"), ("sponsoren", "Sponsoren"), ("kontakt", "Kontakt")):
        if f'id="{sid}"' in teile:
            links.append((f"#{sid}", titel))
    return "\n".join(f'<a href="{h}">{t}</a>' for h, t in links)


def build(web, out):
    web, out = Path(web), Path(out)
    inhalt = json.loads((web / "inhalt.json").read_text(encoding="utf-8"))
    fehler = pruefe(inhalt, web)
    if fehler:
        raise ValueError("web/inhalt.json ungültig:\n  " + "\n  ".join(fehler))
    teile = {
        "HAUPTSPONSOR": abschnitt_haupt(inhalt), "ROBOTER": abschnitt_roboter(inhalt),
        "TEAM": abschnitt_team(inhalt), "SPONSOREN": abschnitt_sponsoren(inhalt),
        "KONTAKT": abschnitt_kontakt(inhalt),
    }
    seite = (web / "index.html").read_text(encoding="utf-8")
    seite = seite.replace("<!--NAV-->", nav("".join(teile.values())))
    for k, v in teile.items():
        seite = seite.replace(f"<!--{k}-->\n", v)
    out.mkdir(parents=True, exist_ok=True)
    (out / "index.html").write_text(seite, encoding="utf-8")
    shutil.copyfile(web / "style.css", out / "style.css")
    if (web / "img").is_dir():
        shutil.copytree(web / "img", out / "img", dirs_exist_ok=True,
                        ignore=shutil.ignore_patterns(".gitkeep"))


def main(argv):
    root = Path(__file__).resolve().parent.parent
    web = Path(argv[0]) if argv else root / "web"
    out = Path(argv[1]) if len(argv) > 1 else root / "site"
    try:
        build(web, out)
    except ValueError as ex:
        print(ex, file=sys.stderr)
        return 1
    print(f"Öffentliche Seite geschrieben: {out / 'index.html'}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
