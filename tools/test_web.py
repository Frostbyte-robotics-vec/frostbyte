"""Tests für die öffentliche Seite web/ (W-02, W-03): Verweise auf Bilder und Anker stimmen, Inhalte nach Vorgabe."""
import re
import unittest
from pathlib import Path

WEB = Path(__file__).resolve().parent.parent / "web"
SEITEN = ("index.html", "sponsoring.html")


def lade(name):
    return re.sub(r"<!--.*?-->", "", (WEB / name).read_text(encoding="utf-8"), flags=re.S)


HTML = {n: lade(n) for n in SEITEN}


class WebTest(unittest.TestCase):
    def test_lokale_dateien_existieren(self):
        for name, html in HTML.items():
            refs = set(re.findall(r'(?:src|href)="(assets/[^"#]+)"', html))
            refs |= set(re.findall(r'url\("(assets/[^"]+)"\)', html))
            self.assertTrue(refs, name)
            fehlt = sorted(r for r in refs if not (WEB / r).is_file())
            self.assertEqual(fehlt, [], name)

    def test_css_verweise_existieren(self):
        css = (WEB / "assets" / "style.css").read_text(encoding="utf-8")
        refs = set(re.findall(r'url\("(assets/[^"]+)"\)', css))
        self.assertEqual(sorted(r for r in refs if not (WEB / r).is_file()), [])

    def test_anker_haben_ziel(self):
        ids = {n: set(re.findall(r'\bid="([^"]+)"', h)) for n, h in HTML.items()}
        for name, html in HTML.items():
            for seite, anker in re.findall(r'href="([a-z]+\.html)?#([^"]+)"', html):
                ziel = seite or name
                self.assertIn(ziel, ids, f"{name}: {seite}")
                self.assertIn(anker, ids[ziel], f"{name}: {seite}#{anker}")

    def test_seitenlinks_zeigen_auf_vorhandene_seiten(self):
        for name, html in HTML.items():
            for seite in re.findall(r'href="([a-z]+\.html)', html):
                self.assertTrue((WEB / seite).is_file(), f"{name}: {seite}")

    def test_bilder_haben_alt_attribut(self):
        for name, html in HTML.items():
            for tag in re.findall(r"<img\b[^>]*>", html):
                self.assertIn("alt=", tag, f"{name}: {tag}")

    def test_nur_ein_head_und_body(self):
        for name, html in HTML.items():
            for t in ("<head>", "<body>", "</html>"):
                self.assertEqual(html.count(t), 1, f"{name}: {t}")

    def test_keine_platzhalter_links_zu_fremden_repos(self):
        for html in HTML.values():
            self.assertNotIn("claude:viewport", html)

    def test_hauptseite_hat_kein_sponsoring_und_keine_werkzeugnamen(self):
        h = HTML["index.html"]
        for id_ in ("pakete", "budget", "partner"):
            self.assertNotIn(f'id="{id_}"', h)
        for wort in ("Fusion 360", "Siemens NX", "CNC", "C++", "Sponsoring-Pakete"):
            self.assertNotIn(wort, h)
        self.assertIn('href="sponsoring.html"', h)  # unauffälliger Weg zur Sponsoringseite

    def test_sponsoringseite_hat_die_abschnitte(self):
        h = HTML["sponsoring.html"]
        for id_ in ("budget", "pakete", "partner", "kontakt"):
            self.assertIn(f'id="{id_}"', h)
        self.assertIn('href="index.html"', h)

    def test_kein_youtube(self):
        for name, html in HTML.items():
            self.assertNotIn("youtube", html.lower(), name)

    def test_skript_prueft_fehlende_elemente(self):
        # Beide Seiten teilen assets/site.js; die Sponsoringseite hat kein Hero (#snow) und keine Anker auf Fremdseiten.
        js = (WEB / "assets" / "site.js").read_text(encoding="utf-8")
        self.assertIn("if(burger&&menu)", js)
        self.assertIn("if(copyBtn)", js)
        self.assertIn('a[href^="#"]', js)


if __name__ == "__main__":
    unittest.main()
