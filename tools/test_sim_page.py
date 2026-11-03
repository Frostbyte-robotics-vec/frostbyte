"""Tests für tools/sim_page.py: erzeugt site/sim/index.html aus dem Viewer (SIM-13)."""
import tempfile
import unittest
from pathlib import Path

import sim_page

ROOT = Path(__file__).resolve().parent.parent
VIEWER = ROOT / "tools" / "viewer" / "index.html"


class SimPageTest(unittest.TestCase):
    def build(self):
        with tempfile.TemporaryDirectory() as d:
            out = Path(d) / "sim" / "index.html"
            sim_page.build(VIEWER, out)
            return out.read_text(encoding="utf-8")

    def test_viewer_hat_genau_einen_body_und_head_anker(self):
        src = VIEWER.read_text(encoding="utf-8")
        self.assertEqual(src.count("<body>"), 1)
        self.assertEqual(src.count("<head>"), 1)

    def test_sim_seite_setzt_ansicht_und_basis(self):
        html = self.build()
        self.assertIn('<body data-ansicht="sim">', html)
        self.assertIn('<base href="../viewer/">', html)
        self.assertNotIn("<body>", html)

    def test_sim_seite_ist_sonst_identisch(self):
        src = VIEWER.read_text(encoding="utf-8")
        html = self.build()
        back = html.replace('<base href="../viewer/">\n', "", 1).replace('<body data-ansicht="sim">', "<body>", 1)
        self.assertEqual(back, src)

    def test_viewer_kennt_den_modus(self):
        src = VIEWER.read_text(encoding="utf-8")
        self.assertIn("body[data-ansicht=\"sim\"]", src)
        self.assertIn("ansicht", src)

    def test_workflow_und_board_seite(self):
        wf = (ROOT / ".github" / "workflows" / "dashboard.yml").read_text(encoding="utf-8")
        self.assertIn("tools/sim_page.py", wf)
        self.assertIn("site/sim/index.html", wf)
        tpl = (ROOT / "tools" / "dashboard" / "template.html").read_text(encoding="utf-8")
        self.assertIn('href="sim/"', tpl)
        self.assertIn("Simulation (intern)", tpl)


if __name__ == "__main__":
    unittest.main()
