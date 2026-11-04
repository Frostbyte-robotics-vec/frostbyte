"""Tests für tools/web_build.py: öffentliche Teamseite aus web/inhalt.json (W-01)."""
import json
import re
import tempfile
import unittest
from pathlib import Path

import web_build

ROOT = Path(__file__).resolve().parent.parent
WEB = ROOT / "web"


def leer():
    return {"sponsoren": [], "team": [], "roboter": [], "kontakt": {}}


def sponsor(**kw):
    s = {"name": "Muster AG", "stufe": "gold", "logo": "muster.svg", "url": "https://example.org/"}
    s.update(kw)
    return s


class PruefungTest(unittest.TestCase):
    def fehler(self, inhalt, logos=("muster.svg",)):
        with tempfile.TemporaryDirectory() as d:
            img = Path(d) / "img" / "sponsoren"
            img.mkdir(parents=True)
            for l in logos:
                (img / l).write_text("<svg/>")
            return web_build.pruefe(inhalt, Path(d))

    def test_leer_ist_gueltig(self):
        self.assertEqual(self.fehler(leer()), [])

    def test_gueltiger_sponsor(self):
        i = leer(); i["sponsoren"] = [sponsor()]
        self.assertEqual(self.fehler(i), [])

    def test_pflichtfelder(self):
        for feld in ("name", "stufe", "logo", "url"):
            i = leer(); s = sponsor(); del s[feld]; i["sponsoren"] = [s]
            self.assertTrue(self.fehler(i), feld)

    def test_ungueltige_stufe(self):
        i = leer(); i["sponsoren"] = [sponsor(stufe="platin")]
        self.assertTrue(self.fehler(i))

    def test_hoechstens_ein_hauptsponsor(self):
        i = leer(); i["sponsoren"] = [sponsor(stufe="haupt"), sponsor(stufe="haupt", name="B")]
        self.assertTrue(self.fehler(i))

    def test_logo_datei_muss_existieren(self):
        i = leer(); i["sponsoren"] = [sponsor(logo="fehlt.svg")]
        self.assertTrue(self.fehler(i))

    def test_url_muss_https_sein(self):
        for url in ("http://example.org", "javascript:alert(1)", "example.org"):
            i = leer(); i["sponsoren"] = [sponsor(url=url)]
            self.assertTrue(self.fehler(i), url)

    def test_logo_pfad_ohne_verzeichniswechsel(self):
        i = leer(); i["sponsoren"] = [sponsor(logo="../x.svg")]
        self.assertTrue(self.fehler(i, logos=("muster.svg",)))

    def test_team_pflichtfelder(self):
        i = leer(); i["team"] = [{"name": "A"}]
        self.assertTrue(self.fehler(i))


class SeiteTest(unittest.TestCase):
    def bauen(self, inhalt):
        with tempfile.TemporaryDirectory() as d:
            web = Path(d) / "web"
            (web / "img" / "sponsoren").mkdir(parents=True)
            (web / "img" / "sponsoren" / "muster.svg").write_text("<svg/>")
            for n in ("index.html", "style.css"):
                (web / n).write_text((WEB / n).read_text(encoding="utf-8"), encoding="utf-8")
            (web / "inhalt.json").write_text(json.dumps(inhalt), encoding="utf-8")
            out = Path(d) / "site"
            web_build.build(web, out)
            dateien = {str(f.relative_to(out)) for f in out.rglob("*") if f.is_file()}
            return (out / "index.html").read_text(encoding="utf-8"), dateien

    def test_leerer_hauptsponsor_zeigt_werden(self):
        html, _ = self.bauen(leer())
        self.assertIn("Hauptsponsor werden", html)

    def test_hauptsponsor_steht_oben_mit_logo_und_link(self):
        i = leer(); i["sponsoren"] = [sponsor(stufe="haupt", name="Haupt GmbH"), sponsor()]
        html, _ = self.bauen(i)
        self.assertNotIn("Hauptsponsor werden", html)
        self.assertLess(html.index("Haupt GmbH"), html.index("Muster AG"))
        self.assertRegex(html, r'<img[^>]+alt="[^"]*Haupt GmbH')
        self.assertIn('href="https://example.org/"', html)

    def test_leere_listen_blenden_abschnitte_aus(self):
        html, _ = self.bauen(leer())
        self.assertNotIn('id="team"', html)
        self.assertNotIn('id="roboter"', html)
        self.assertNotIn("Gold", html)
        self.assertNotIn("Muster", html)

    def test_sponsoren_nach_stufe(self):
        i = leer(); i["sponsoren"] = [sponsor(stufe="silber", name="S"), sponsor(stufe="gold", name="G")]
        html, _ = self.bauen(i)
        self.assertLess(html.index(">G<"), html.index(">S<"))

    def test_html_wird_escaped(self):
        i = leer(); i["team"] = [{"name": "<b>x</b>", "rolle": "a&b"}]
        html, _ = self.bauen(i)
        self.assertNotIn("<b>x</b>", html)
        self.assertIn("a&amp;b", html)

    def test_alle_bilder_haben_alt(self):
        i = leer(); i["sponsoren"] = [sponsor(), sponsor(stufe="haupt", name="H")]
        html, _ = self.bauen(i)
        for tag in re.findall(r"<img\b[^>]*>", html):
            self.assertRegex(tag, r'alt="[^"]+"')

    def test_logos_und_css_werden_kopiert(self):
        i = leer(); i["sponsoren"] = [sponsor()]
        _, dateien = self.bauen(i)
        self.assertIn("style.css", dateien)
        self.assertIn("img/sponsoren/muster.svg", dateien)


class OhneAnimationTest(unittest.TestCase):
    def test_echte_seite_ohne_animation_schrift_skript(self):
        with tempfile.TemporaryDirectory() as d:
            web_build.build(WEB, Path(d))
            html = (Path(d) / "index.html").read_text(encoding="utf-8")
            css = (Path(d) / "style.css").read_text(encoding="utf-8")
        alles = (html + css).lower()
        for verboten in ("@keyframes", "transition", "animation", "<script", "@import",
                         "@font-face", "fonts.googleapis", "gradient", "box-shadow", "backdrop-filter",
                         "parallax", "scroll-behavior"):
            self.assertNotIn(verboten, alles, verboten)
        self.assertNotRegex(html, r'src="https?://')
        self.assertNotRegex(html, r'<link[^>]+https?://')
        self.assertIn("system-ui", css)

    def test_echte_inhalt_json_ist_gueltig(self):
        inhalt = json.loads((WEB / "inhalt.json").read_text(encoding="utf-8"))
        self.assertEqual(web_build.pruefe(inhalt, WEB), [])

    def test_echte_seite_hat_ausgeglichene_tags(self):
        from html.parser import HTMLParser
        leer_tags = {"meta", "link", "img", "br", "hr", "input"}

        class P(HTMLParser):
            def __init__(self):
                super().__init__()
                self.stack, self.fehler = [], []

            def handle_starttag(self, tag, attrs):
                if tag not in leer_tags:
                    self.stack.append(tag)

            def handle_endtag(self, tag):
                if not self.stack or self.stack.pop() != tag:
                    self.fehler.append(tag)

        with tempfile.TemporaryDirectory() as d:
            web_build.build(WEB, Path(d))
            p = P()
            p.feed((Path(d) / "index.html").read_text(encoding="utf-8"))
        self.assertEqual(p.fehler, [])
        self.assertEqual(p.stack, [])

    def test_echte_seite_ohne_eth_logo(self):
        self.assertFalse(list((WEB / "img").rglob("*eth*")))


if __name__ == "__main__":
    unittest.main()
