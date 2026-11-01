"""Tests für make_test_logs.py: Testlogs müssen exakt docs/log_format.md folgen.

Ausführen: python3 -m unittest discover -s tools/viewer -p "test_*.py"
"""
import csv
import io
import re
import tempfile
import unittest
from pathlib import Path

import make_test_logs as mtl

ROOT = Path(__file__).resolve().parents[2]
LOG_FORMAT = ROOT / "docs" / "log_format.md"


def spec_columns():
    """Spaltennamen in Reihenfolge aus der Tabelle in docs/log_format.md."""
    cols = []
    for line in LOG_FORMAT.read_text(encoding="utf-8").splitlines():
        if not line.startswith("| ") or line.startswith("| Spalte") or line.startswith("| ---"):
            continue
        cols += [c.strip().strip("`") for c in line.split("|")[1].split(",")]
    return cols


def spec_version():
    m = re.search(r"Log-Format v(\d+)", LOG_FORMAT.read_text(encoding="utf-8"))
    return int(m.group(1))


def parse(text):
    lines = text.splitlines()
    rows = list(csv.DictReader(io.StringIO("\n".join(lines[1:]))))
    return lines[0], lines[1].split(","), rows


class TestSpec(unittest.TestCase):
    def test_spalten_aus_spec_gelesen(self):
        cols = spec_columns()
        self.assertEqual(cols[0], "t_s")
        self.assertEqual(cols[-1], "event")
        self.assertEqual(len(cols), len(set(cols)))

    def test_generator_kennt_dieselbe_version(self):
        self.assertEqual(mtl.VERSION, spec_version())


class TestLogs(unittest.TestCase):
    def setUp(self):
        self.sim = mtl.make_log("stacker", "sim", seed=7)
        self.rob = mtl.make_log("stacker", "robot", seed=7)
        self.run = mtl.make_log("runner", "sim", seed=7)

    def test_kopfzeile(self):
        head, _, _ = parse(self.sim)
        self.assertRegex(head, r"^# frostbyte-log v%d robot=stacker source=sim seed=7 build=\S+$" % mtl.VERSION)
        head, _, _ = parse(self.rob)
        self.assertRegex(head, r"^# frostbyte-log v%d robot=stacker source=robot seed=- build=\S+$" % mtl.VERSION)

    def test_spalten_wie_spec(self):
        for text in (self.sim, self.rob, self.run):
            _, cols, _ = parse(text)
            self.assertEqual(cols, spec_columns())

    def test_takt_20ms_ganzes_match(self):
        _, _, rows = parse(self.sim)
        ts = [float(r["t_s"]) for r in rows]
        self.assertEqual(ts[0], 0.0)
        self.assertAlmostEqual(ts[-1], 60.0)
        self.assertEqual(len(ts), 3001)
        for a, b in zip(ts, ts[1:]):
            self.assertAlmostEqual(b - a, 0.02, places=6)

    def test_robot_spalte_passt_zur_kopfzeile(self):
        for text, name in ((self.sim, "stacker"), (self.run, "runner")):
            _, _, rows = parse(text)
            self.assertEqual({r["robot"] for r in rows}, {name})

    def test_wahre_pose_nur_in_simulation(self):
        _, _, rows = parse(self.rob)
        self.assertTrue(all(r["tx_in"] == r["ty_in"] == r["th_deg"] == "" for r in rows))
        _, _, rows = parse(self.sim)
        self.assertTrue(all(r["tx_in"] != "" for r in rows))

    def test_runner_ohne_arm(self):
        _, _, rows = parse(self.run)
        self.assertTrue(all(r["arm_deg"] == "" and r["arm_set_deg"] == "" for r in rows))

    def test_gps_teilweise_leer(self):
        _, _, rows = parse(self.rob)
        leer = sum(r["gx_in"] == "" for r in rows)
        self.assertGreater(leer, 0)
        self.assertLess(leer, len(rows))

    def test_werte_gueltig(self):
        _, _, rows = parse(self.sim)
        zyklen = {"BEREIT", "AUFNEHMEN", "GREIFEN", "HEBEN", "AM_GOAL", "SETZEN", "RUECKZUG", "FEHLER"}
        for r in rows:
            self.assertIn(r["cycle"], zyklen)
            self.assertIn(r["has_cup"], ("0", "1"))
            self.assertLessEqual(abs(float(r["x_in"])), 72)
            self.assertLessEqual(abs(float(r["y_in"])), 72)

    def test_ereignisse_vorhanden_und_wohlgeformt(self):
        _, _, rows = parse(self.sim)
        events = [e for r in rows if r["event"] for e in r["event"].split(";")]
        self.assertTrue(any(e.startswith("PLACED:") for e in events))
        for e in events:
            self.assertRegex(e, r"^(TIMEOUT:\w+|BLOCKED|COLLISION:\w+|RULE:\w+|PLACED:\w+:\d+)$")

    def test_kein_komma_in_zellen(self):
        _, cols, _ = parse(self.sim)
        for line in self.sim.splitlines()[2:]:
            self.assertEqual(len(line.split(",")), len(cols))

    def test_gleicher_seed_bytegleich(self):
        self.assertEqual(self.sim, mtl.make_log("stacker", "sim", seed=7))
        self.assertNotEqual(self.sim, mtl.make_log("stacker", "sim", seed=8))

    def test_main_schreibt_dateien(self):
        with tempfile.TemporaryDirectory() as d:
            mtl.main(["--out", d, "--seed", "3"])
            namen = sorted(p.name for p in Path(d).iterdir())
            self.assertEqual(namen, ["robot_stacker.csv", "sim_runner.csv", "sim_stacker.csv"])


if __name__ == "__main__":
    unittest.main()
