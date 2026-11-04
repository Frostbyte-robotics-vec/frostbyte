"""Tests für die öffentliche Seite web/ (W-02): Verweise auf Bilder und Anker stimmen."""
import re
import unittest
from pathlib import Path

WEB = Path(__file__).resolve().parent.parent / "web"
HTML = re.sub(r"<!--.*?-->", "", (WEB / "index.html").read_text(encoding="utf-8"), flags=re.S)


class WebTest(unittest.TestCase):
    def test_lokale_dateien_existieren(self):
        refs = set(re.findall(r'(?:src|href)="(assets/[^"#]+)"', HTML))
        refs |= set(re.findall(r'url\("(assets/[^"]+)"\)', HTML))
        self.assertTrue(refs)
        fehlt = sorted(r for r in refs if not (WEB / r).is_file())
        self.assertEqual(fehlt, [])

    def test_anker_haben_ziel(self):
        ids = set(re.findall(r'\bid="([^"]+)"', HTML))
        links = set(re.findall(r'href="#([^"]+)"', HTML))
        self.assertEqual(sorted(links - ids), [])

    def test_bilder_haben_alt_attribut(self):
        for tag in re.findall(r"<img\b[^>]*>", HTML):
            self.assertIn("alt=", tag, tag)

    def test_nur_ein_head_und_body(self):
        for t in ("<head>", "<body>", "</html>"):
            self.assertEqual(HTML.count(t), 1, t)

    def test_keine_platzhalter_links_zu_fremden_repos(self):
        self.assertNotIn("claude:viewport", HTML)


if __name__ == "__main__":
    unittest.main()
