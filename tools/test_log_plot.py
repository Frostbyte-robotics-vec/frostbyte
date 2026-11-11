import os
import tempfile
import unittest

import log_plot

HEAD = "# frostbyte-log v1 robot=runner source=robot seed=- build=abc\n"
COLS = ",".join(log_plot.COLUMNS) + "\n"


def row(t, x="", y="", cpu="", event=""):
    cells = [""] * len(log_plot.COLUMNS)
    cells[0] = t
    cells[1] = "runner"
    cells[2] = x
    cells[3] = y
    cells[log_plot.COLUMNS.index("cpu_ms")] = cpu
    cells[-1] = event
    return ",".join(cells) + "\n"


class LogPlotTest(unittest.TestCase):
    def write(self, text):
        d = tempfile.mkdtemp()
        self.addCleanup(lambda: __import__("shutil").rmtree(d, ignore_errors=True))
        p = os.path.join(d, "l.csv")
        with open(p, "w", encoding="utf-8", newline="") as f:
            f.write(text)
        return p

    def test_liest_zeilen_leere_zelle_ist_none(self):
        p = self.write(HEAD + COLS + row("0.000", cpu="0.250") + row("0.020", x="1.5", y="-2", event="PHASE:DRIVER"))
        head, rows = log_plot.read_log(p)
        self.assertEqual(head["robot"], "runner")
        self.assertEqual(len(rows), 2)
        self.assertIsNone(rows[0]["x_in"])
        self.assertEqual(rows[1]["x_in"], 1.5)
        self.assertEqual(rows[0]["cpu_ms"], 0.25)

    def test_ereignisse_mehrere_je_zeile_und_unbekannte_token(self):
        p = self.write(HEAD + COLS + row("1.000", event="BLOCKED;LOGDROP:3;NEU:ding"))
        _, rows = log_plot.read_log(p)
        self.assertEqual([t for _, t in log_plot.events(rows)], ["BLOCKED", "LOGDROP:3", "NEU:ding"])

    def test_falsche_kopfzeile_spalten_und_zahl_werden_abgelehnt(self):
        for text in ("# anderes\n" + COLS, HEAD + "t_s,robot\n", HEAD + COLS + "x," + "," * 26 + "\n",
                     HEAD + COLS + "0.0,runner\n"):
            with self.assertRaises(log_plot.LogError):
                log_plot.read_log(self.write(text))

    def test_html_enthaelt_diagramm_nur_fuer_gefuellte_spalten(self):
        text = HEAD + COLS + "".join(row("%.3f" % (i * 0.02), x=str(i), y=str(-i), cpu="0.5") for i in range(10))
        p = self.write(text)
        head, rows = log_plot.read_log(p)
        page = log_plot.build_html(p, head, rows)
        self.assertIn("<polyline", page)
        self.assertIn("cpu_ms", page)
        self.assertNotIn("lift_deg", page)
        self.assertIn("10 Zeilen", page)

    def test_main_schreibt_html_und_gibt_1_bei_fehler(self):
        p = self.write(HEAD + COLS + row("0.0", cpu="1") + row("0.02", cpu="2"))
        out = p[:-4] + ".html"
        self.assertEqual(log_plot.main([p]), 0)
        self.assertTrue(os.path.exists(out))
        self.assertEqual(log_plot.main([p + ".fehlt"]), 1)


if __name__ == "__main__":
    unittest.main()
