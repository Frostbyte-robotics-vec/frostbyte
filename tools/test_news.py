"""Tests für tools/news_build.py (W-06): Beiträge als Textdateien werden zu Seiten im Design der Hauptseite."""
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import news_build as nb

BEITRAG = """titel: Novagear ist Hauptpartner
datum: 2026-11-09
kategorie: Sponsoren

Wir freuen uns über **Novagear**.

## Was das heisst
- Material
- Unterstützung

Mehr unter [novagear](https://example.org/x?a=1&b=2).
"""


def schreibe(ordner, name, text):
    (ordner / "beitraege").mkdir(exist_ok=True)
    (ordner / "beitraege" / name).write_text(text, encoding="utf-8")


class NewsTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.src = Path(self.tmp.name) / "news"
        self.src.mkdir()
        self.out = Path(self.tmp.name) / "site" / "news"

    def tearDown(self):
        self.tmp.cleanup()

    def test_parse_kopf_und_text(self):
        p = nb.parse(BEITRAG, "2026-11-09-novagear.md")
        self.assertEqual(p["titel"], "Novagear ist Hauptpartner")
        self.assertEqual(p["datum"], "2026-11-09")
        self.assertEqual(p["kategorie"], "Sponsoren")
        self.assertEqual(p["slug"], "2026-11-09-novagear")
        self.assertIn("Wir freuen uns", p["text"])

    def test_markdown_teilmenge(self):
        h = nb.render_text("Ein **fetter** Satz.\n\n## Kopf\n- a\n- b\n\n[x](https://e.org/?a=1&b=2)")
        self.assertIn("<p>Ein <strong>fetter</strong> Satz.</p>", h)
        self.assertIn("<h2>Kopf</h2>", h)
        self.assertIn("<ul><li>a</li><li>b</li></ul>", h)
        self.assertIn('<a href="https://e.org/?a=1&amp;b=2" rel="noopener">x</a>', h)

    def test_html_im_text_wird_maskiert(self):
        h = nb.render_text('<script>alert(1)</script> [böse](javascript:alert(1))')
        self.assertNotIn("<script>", h)
        self.assertNotIn("javascript:", h)

    def test_fehler_nennen_dateiname(self):
        for text in ("datum: 2026-11-09\n\nx", "titel: A\ndatum: 09.11.2026\nkategorie: Team\n\nx",
                     "titel: A\ndatum: 2026-02-30\nkategorie: Team\n\nx", "titel: A\ndatum: 2026-11-09\n\nx"):
            with self.assertRaises(nb.NewsFehler) as e:
                nb.parse(text, "2026-11-09-kaputt.md")
            self.assertIn("2026-11-09-kaputt.md", str(e.exception))

    def test_unbekannte_kategorie_ist_fehler(self):
        with self.assertRaises(nb.NewsFehler):
            nb.parse("titel: A\ndatum: 2026-11-09\nkategorie: Quatsch\n\nx", "2026-11-09-a.md")

    def test_bau_neueste_zuerst_und_entwurf_fehlt(self):
        schreibe(self.src, "2026-10-01-alt.md", "titel: Alt\ndatum: 2026-10-01\nkategorie: Team\n\nalt")
        schreibe(self.src, "2026-11-09-novagear.md", BEITRAG)
        schreibe(self.src, "2026-11-10-geheim.md",
                 "titel: Geheim\ndatum: 2026-11-10\nkategorie: Team\nentwurf: ja\n\nx")
        nb.build(self.src, self.out)
        index = (self.out / "index.html").read_text(encoding="utf-8")
        self.assertLess(index.index("Novagear ist Hauptpartner"), index.index("Alt"))
        self.assertNotIn("Geheim", index)
        self.assertTrue((self.out / "2026-11-09-novagear.html").is_file())
        self.assertFalse((self.out / "2026-11-10-geheim.html").exists())

    def test_leere_liste_ehrlicher_hinweis(self):
        nb.build(self.src, self.out)
        index = (self.out / "index.html").read_text(encoding="utf-8")
        self.assertIn("Noch keine Beiträge", index)

    def test_seiten_nutzen_gemeinsame_dateien_der_hauptseite(self):
        schreibe(self.src, "2026-11-09-novagear.md", BEITRAG)
        nb.build(self.src, self.out)
        for name in ("index.html", "2026-11-09-novagear.html"):
            h = (self.out / name).read_text(encoding="utf-8")
            self.assertIn('href="../assets/style.css"', h)
            self.assertNotIn("sponsoring.html", h)
            self.assertNotIn("fonts.googleapis", h)
            self.assertNotIn("progress", h)
            self.assertEqual(h.count("<body>"), 1)

    def test_bild_wird_kopiert_und_hat_alt(self):
        (self.src / "bilder").mkdir()
        (self.src / "bilder" / "foto.jpg").write_bytes(b"x")
        schreibe(self.src, "2026-11-09-novagear.md", BEITRAG.replace("kategorie: Sponsoren",
                 "kategorie: Events\nbild: foto.jpg\nbild_alt: Das Team am Stand"))
        nb.build(self.src, self.out)
        h = (self.out / "2026-11-09-novagear.html").read_text(encoding="utf-8")
        self.assertIn('src="bilder/foto.jpg" alt="Das Team am Stand"', h)
        self.assertTrue((self.out / "bilder" / "foto.jpg").is_file())

    def test_fehlendes_bild_oder_alt_ist_fehler(self):
        schreibe(self.src, "2026-11-09-a.md", "titel: A\ndatum: 2026-11-09\nkategorie: Team\nbild: weg.jpg\nbild_alt: x\n\nx")
        with self.assertRaises(nb.NewsFehler):
            nb.build(self.src, self.out)

    def test_pfad_trick_im_bild_abgelehnt(self):
        with self.assertRaises(nb.NewsFehler):
            nb.parse("titel: A\ndatum: 2026-11-09\nkategorie: Team\nbild: ../x.jpg\nbild_alt: a\n\nx", "2026-11-09-a.md")

    def test_dateiname_muss_dem_muster_folgen(self):
        text = "titel: A\ndatum: 2026-11-09\nkategorie: Team\n\nx"
        for name in ("a.md", "2026-12-05 Event Zürich.md", '2026-12-05-a"b.md', "2026-12-05-Gross.md", "2026-12-05-a.txt"):
            with self.assertRaises(nb.NewsFehler, msg=name) as e:
                nb.parse(text, name)
            self.assertIn(name, str(e.exception))


if __name__ == "__main__":
    unittest.main()
