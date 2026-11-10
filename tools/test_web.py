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

    def test_kontakt_ist_die_teamadresse_ohne_personenname(self):
        for name, html in HTML.items():
            self.assertNotIn("lechnerm", html, name)
            self.assertIn("frostbyte.eth.robotics@gmail.com", html, name)
            self.assertNotIn("Lechner · Frostbyte", html, name)
        self.assertNotIn("lechnerm", (WEB / "assets" / "site.js").read_text(encoding="utf-8"))
        self.assertIn("frostbyte.eth.robotics@gmail.com", (WEB / "assets" / "site.js").read_text(encoding="utf-8"))

    def test_livestream_zahl_und_fusszeile(self):
        self.assertIn("160'000", HTML["index.html"])
        for html in HTML.values():
            self.assertNotIn("Ziel: Top 16", html)
            self.assertNotIn("Top 16", html)

    def test_social_links_sind_echt(self):
        h = HTML["index.html"]
        self.assertIn('href="https://www.instagram.com/frostbyte.robotics/"', h)
        self.assertIn('href="https://www.linkedin.com/company/frostbyterobotics"', h)
        for name, html in HTML.items():
            self.assertNotIn('href="#"', html, name)

    def test_freie_fotos_haben_bildnachweis(self):
        h = HTML["index.html"]
        for datei in ("worlds-match.jpg", "team-trophy.jpg"):
            self.assertIn(f"assets/{datei}", h)
        self.assertIn("Rick Tyler", h)
        self.assertIn("CC BY-SA 3.0", h)
        self.assertIn("Wrc Ynapmoc", h)
        self.assertIn("CC BY 3.0", h)

    def test_mosaik_fotos_haben_bildnachweis_und_alte_dateien_sind_weg(self):
        h = HTML["index.html"]
        for i in (1, 2, 4):
            self.assertIn(f"assets/liga-{i}.jpg", h)
            self.assertTrue((WEB / "assets" / f"liga-{i}.jpg").is_file())
        self.assertIn("assets/arena.jpg", h)  # Entscheidung des Menschen: Arena-Foto aus der eigenen Vorlage, siehe ADR-036
        for titel in ("One last adjustment", "VEX Starstruck Standard Fields", "Judgment day at Sunset Station"):
            self.assertIn(titel, h)
        self.assertIn("Sean Spicer", h)
        self.assertIn("CC BY-SA 4.0", h)
        self.assertIn("Lisa Litchfield", h)
        # Herkunft und Rechte undokumentiert: darf nicht zurückkommen
        for alt in ("event-field", "vex-stage", "champions.jpg"):
            self.assertNotIn(alt, h)
            self.assertFalse((WEB / "assets" / alt).exists() if alt.endswith(".jpg") else False)

    def test_skript_prueft_fehlende_elemente(self):
        # Beide Seiten teilen assets/site.js; die Sponsoringseite hat kein Hero (#snow) und keine Anker auf Fremdseiten.
        js = (WEB / "assets" / "site.js").read_text(encoding="utf-8")
        self.assertIn("if(burger&&menu)", js)
        self.assertIn("if(copyBtn)", js)
        self.assertIn('a[href^="#"]', js)

    def test_seiten_verweisen_auf_news_in_navigation_und_fusszeile(self):
        for name in SEITEN:
            html = HTML[name]
            nav, fuss = html.split("</nav>")[0], html.split("<footer")[1]
            self.assertIn('href="news/index.html"', nav, name)
            self.assertIn('href="news/index.html"', fuss, name)

    def test_sponsor_norelem_schreibweise(self):
        h = HTML["sponsoring.html"]
        self.assertIn("norelem", h)
        for falsch in ("NORELEM", "Norelem"):
            self.assertNotIn(falsch, h)

    def test_hauptsponsor_novagear_schreibweise(self):
        h = HTML["sponsoring.html"]
        self.assertIn("Novagear", h)
        self.assertNotIn("NOVAGEAR", h)


if __name__ == "__main__":
    unittest.main()
