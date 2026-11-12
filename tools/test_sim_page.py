"""Tests für tools/sim_page.py: erzeugt site/sim/index.html aus dem Viewer (SIM-13)."""
import re
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

    def sim_css(self):
        """Alle CSS-Regeln, deren Selektor mit body[data-ansicht="sim"] beginnt (Regel -> Rumpf)."""
        src = VIEWER.read_text(encoding="utf-8")
        style = src[src.index("<style>"):src.index("</style>")]
        return {sel.strip(): body for sel, body in re.findall(r'(body\[data-ansicht="sim"\][^{}]*)\{([^{}]*)\}', style)}

    def hidden_in_sim(self, css, ziel):
        return any(ziel in sel and "display:none" in body.replace(" ", "") for sel, body in css.items())

    def test_sim_blendet_feldinfo_zeitleiste_log_und_hinweise_aus(self):
        css = self.sim_css()
        for ziel in ("#fieldinfo", ".time", "#logsCard", "#evCard", "#empty", ".hint"):
            self.assertTrue(self.hidden_in_sim(css, ziel), ziel)

    def test_sim_log_spalte_ist_eine_eigene_karte(self):
        src = VIEWER.read_text(encoding="utf-8")
        self.assertIn('id="evCard"', src)

    def test_sim_hintergrund_ist_weiss_auch_im_dunklen_design(self):
        css = self.sim_css()
        body = css.get('body[data-ansicht="sim"]', "").replace(" ", "").lower()
        for var in ("--paper:#ffffff", "--viewer:#ffffff", "color-scheme:light"):
            self.assertIn(var, body)

    def test_sim_ohne_rahmen_und_einspaltig(self):
        css = self.sim_css()
        v = " ".join(b for s, b in css.items() if ".viewer" in s).replace(" ", "")
        self.assertIn("border:0", v)
        self.assertIn("box-shadow:none", v)
        m = " ".join(b for s, b in css.items() if ".main" in s).replace(" ", "")
        self.assertIn("grid-template-columns:minmax(0,1fr)", m)

    def test_sim_startet_direkt_live_ohne_demo(self):
        src = VIEWER.read_text(encoding="utf-8")
        self.assertIn("const SIM = ", src)
        self.assertIn("if (SIM) startLive()", src)
        self.assertIn("SIM || q.has('nodemo')", src)

    def test_sim_liveinfo_hat_nur_eine_zeile(self):
        src = VIEWER.read_text(encoding="utf-8")
        i = src.index("function updateLive()")
        body = src[i:src.index("async function startLive", i)]
        self.assertIn("SIM ? head :", body)
        head = body[body.index("const head = "):body.index("$('liveinfo').textContent")]
        self.assertNotIn("\\n", head)
        # Tastenhilfe und Ersatzpose-Hinweise stehen nur noch in der Viewer-Variante
        self.assertLess(body.index("SIM ? head :"), body.index("W/S fahren"))

    def test_viewer_zeigt_weiter_alles(self):
        # Ausblenden nur unter body[data-ansicht="sim"]; ohne den Modus gibt es keine display:none-Regel dafür
        src = VIEWER.read_text(encoding="utf-8")
        style = src[src.index("<style>"):src.index("</style>")]
        for ziel in ("#fieldinfo", ".time{", "#evCard"):
            for zeile in style.splitlines():
                if zeile.startswith("body[data-ansicht") or "display:none" not in zeile.replace(" ", ""):
                    continue
                self.assertNotIn(ziel, zeile.split("{")[0], zeile)

    def test_workflow_und_board_seite(self):
        wf = (ROOT / ".github" / "workflows" / "dashboard.yml").read_text(encoding="utf-8")
        self.assertIn("tools/sim_page.py", wf)  # W-17: /sim/ ist wieder öffentlich
        self.assertIn("site/sim/index.html", wf)
        tpl = (ROOT / "tools" / "dashboard" / "template.html").read_text(encoding="utf-8")
        self.assertIn('href="../sim/"', tpl)  # Board liegt unter /board/ (ADR-035)
        self.assertIn("Simulation (intern)", tpl)


    # ---- SIM-21: Kamera verschieben (Mausrad-Taste) und mit R zurücksetzen. Statische Prüfung: im Repo gibt es keinen
    # Browser-Test (kein Playwright), das Verhalten im Browser steht in der Übergabe von tasks/SIM-21.md. ----

    def viewer(self):
        return VIEWER.read_text(encoding="utf-8")

    def handler(self, src, start):
        """Rumpf des keydown-Handlers, der mit `start` beginnt, bis zur nächsten Zeile `});` am Zeilenanfang."""
        i = src.index(start)
        return src[i:src.index("\n});", i)] + "\n"

    def test_mittlere_maustaste_verschiebt_die_kamera(self):
        src = self.viewer()
        self.assertIn("controls.mouseButtons", src)
        self.assertRegex(src, r"MIDDLE:\s*THREE\.MOUSE\.PAN")
        self.assertRegex(src, r"LEFT:\s*THREE\.MOUSE\.ROTATE")
        self.assertRegex(src, r"RIGHT:\s*THREE\.MOUSE\.PAN")

    def test_mittlere_maustaste_loest_kein_autoscroll_aus(self):
        src = self.viewer()
        self.assertRegex(src, r"addEventListener\('mousedown',[^\n]*button\s*===\s*1[^\n]*preventDefault")

    def test_r_setzt_die_kamera_zurueck_in_live_und_wiedergabe(self):
        src = self.viewer()
        play = self.handler(src, "window.addEventListener('keydown', e => {\n  if (e.target.tagName === 'INPUT' && e.target.type")
        i = play.index("KeyR")
        r = play[i:play.index("\n", i)]
        self.assertIn("!e.repeat", r)
        self.assertIn("setView(", r)
        self.assertIn("aria-pressed') === 'true'", r)  # aktuelle Ansicht (oben/schräg) behalten
        self.assertIn("S.follow = false", r)
        self.assertIn("bFollow", r)

    def test_roboter_zuruecksetzen_liegt_auf_backspace_nicht_mehr_auf_r(self):
        src = self.viewer()
        live = self.handler(src, "window.addEventListener('keydown', e => {\n  if (!S.live ||")
        self.assertNotIn("KeyR", live)
        i = live.index("Backspace")
        line = live[i:live.index("\n", i)]
        self.assertIn("!e.repeat", line)
        self.assertIn("S.live.sim.reset()", line)
        self.assertIn("preventDefault", line)

    def test_tastenhilfe_nennt_neue_belegung(self):
        src = self.viewer()
        self.assertIn("Mittlere Taste ziehen: schieben", src)
        self.assertEqual(src.count("Rücktaste zurücksetzen"), 2)  # #liveinfo und title von #bLive
        self.assertNotIn("R zurücksetzen", src)
        self.assertIn("R Kamera zurück", src)


if __name__ == "__main__":
    unittest.main()
