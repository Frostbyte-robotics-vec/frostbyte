"""Führt die JS-Tests des Viewers (logparse_test.js) unter JavaScriptCore aus (osascript, nur macOS).

Ausführen: python3 -m unittest discover -s tools/viewer -p "test_*.py"
Ohne osascript wird übersprungen; dann im Browser tools/viewer/index.html?selftest öffnen.
"""
import json
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

import make_test_logs as mtl

HERE = Path(__file__).resolve().parent


def run_js(body):
    src = (HERE / "logparse.js").read_text(encoding="utf-8") + "\n" + \
        (HERE / "logparse_test.js").read_text(encoding="utf-8") + "\n" + body
    with tempfile.NamedTemporaryFile("w", suffix=".js", delete=False, encoding="utf-8") as f:
        f.write(src)
    try:
        r = subprocess.run(["osascript", "-l", "JavaScript", f.name], capture_output=True, text=True, timeout=60)
    finally:
        Path(f.name).unlink()
    if r.returncode != 0:
        raise AssertionError(r.stderr)
    return json.loads(r.stdout)


@unittest.skipUnless(shutil.which("osascript"), "osascript fehlt (nur macOS)")
class TestLogparse(unittest.TestCase):
    def test_js_einheitentests(self):
        res = run_js("JSON.stringify(runLogparseTests(FBLog));")
        self.assertEqual(res["fail"], 0, "\n".join(res["msgs"]))
        self.assertGreater(res["ok"], 30)

    def test_generierte_logs(self):
        sim = mtl.make_log("stacker", "sim", seed=5)
        rob = mtl.make_log("stacker", "robot", seed=5)
        res = run_js(f"""
const a = FBLog.parseLog({json.dumps(sim)}), b = FBLog.parseLog({json.dumps(rob)});
const A = a.robots.stacker, B = b.robots.stacker;
JSON.stringify({{
  warn: a.warnings.concat(b.warnings), na: A.n, nb: B.n, src: [a.meta.source, b.meta.source],
  txB: Array.from(B.num.tx_in).every(Number.isNaN), txA: Array.from(A.num.tx_in).some(Number.isNaN),
  range: FBLog.timeRange(a), placed: FBLog.events(a).filter(e => e.kind === 'PLACED').length,
  dx: Math.abs(FBLog.sample(A, 'tx_in', 30.01) - FBLog.sample(B, 'x_in', 30.01))
}});""")
        self.assertEqual(res["warn"], [])
        self.assertEqual((res["na"], res["nb"]), (3001, 3001))
        self.assertEqual(res["src"], ["sim", "robot"])
        self.assertTrue(res["txB"])
        self.assertFalse(res["txA"])
        self.assertEqual(res["range"], [0, 60])
        self.assertGreater(res["placed"], 0)
        # Gleiche wahre Bahn, Roboter-Odometrie driftet nur wenig ab.
        self.assertLess(res["dx"], 6.0)


if __name__ == "__main__":
    unittest.main()
