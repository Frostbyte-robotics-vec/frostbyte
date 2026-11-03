"""Führt die JS-Tests der Live-Handsteuerung (livesim_test.js) unter Node aus und prüft, dass livesim.js die echte
sim/sim_params.json lesen kann (nur Struktur, keine Werte).

Ausführen: python3 -m unittest discover -s tools/viewer -p "test_livesim.py"
Ohne node wird übersprungen; dann im Browser tools/viewer/index.html?selftest öffnen.
"""
import json
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

import make_test_logs as mtl

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
POS_TOL_IN = 0.1     # Toleranz laut SIM-17
HEAD_TOL_DEG = 0.5


def run_node(body):
    src = (HERE / "logparse.js").read_text(encoding="utf-8") + "\n" + \
        (HERE / "livesim.js").read_text(encoding="utf-8") + "\n" + \
        (HERE / "livesim_test.js").read_text(encoding="utf-8") + "\n" + body
    with tempfile.NamedTemporaryFile("w", suffix=".js", delete=False, encoding="utf-8") as f:
        f.write(src)
    try:
        r = subprocess.run(["node", f.name], capture_output=True, text=True, timeout=60, encoding="utf-8")
    finally:
        Path(f.name).unlink()
    if r.returncode != 0:
        raise AssertionError(r.stderr)
    return json.loads(r.stdout)


class TestLivesim(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not shutil.which("node"):
            if os.environ.get("CI"):
                raise AssertionError("node fehlt in der CI")   # dort nie stillschweigend überspringen
            raise unittest.SkipTest("node fehlt")

    def test_js_einheitentests(self):
        res = run_node("console.log(JSON.stringify(runLivesimTests(FBLive, FBLog)));")
        self.assertEqual(res["fail"], 0, "\n".join(res["msgs"]))
        self.assertGreater(res["ok"], 30)

    def test_spalten_wie_log_format(self):
        res = run_node("console.log(JSON.stringify(FBLive.LOG_COLUMNS));")
        self.assertEqual(res, mtl.COLUMNS)

    def test_echte_parameter_und_feld(self):
        params = ROOT / "sim" / "sim_params.json"
        field = ROOT / "field" / "field.json"
        res = run_node(f"""
const params = {params.read_text(encoding="utf-8")}, field = {field.read_text(encoding="utf-8")};
const live = FBLive.create(params, field);
live.setKeys({{ w: true }});
for (let i = 0; i < 100; i++) live.advance(0.01);
console.log(JSON.stringify({{ notes: live.notes.length, pose: live.pose('stacker') }}));""")
        self.assertLessEqual(res["notes"], 2)
        self.assertTrue(all(abs(v) < 1e6 for v in res["pose"].values()))

    def test_abgleich_mit_frostbyte_sim(self):
        """Gleicher Eingabeverlauf wie frostbyte_sim (test/fixtures): wahre Pose ≤ 0,1 in / 0,5°, gleiche Kollisionen."""
        fx = ROOT / "test" / "fixtures"
        params = json.loads((fx / "sim_params_min.json").read_text(encoding="utf-8"))
        # Nur der Browser-Block fehlt in der Fixture; kein live.kollision: das C++-Modell kennt keine Feldelemente.
        params["live"] = {"langsam_faktor": 0.4, "ersatzpose_in": {"stacker": {"x_in": 0, "y_in": 0, "h_deg": 0},
                                                                   "runner": {"x_in": 0, "y_in": 0, "h_deg": 0}}}
        data = {
            "params": params,
            "field": json.loads((fx / "field_two.json").read_text(encoding="utf-8")),
            "input": (fx / "livesim_input.csv").read_text(encoding="utf-8"),
            "reference": (fx / "livesim_reference.csv").read_text(encoding="utf-8"),
        }
        res = run_node(f"console.log(JSON.stringify(runLivesimReferenceTest(FBLive, {json.dumps(data)}, {POS_TOL_IN}, {HEAD_TOL_DEG})));")
        self.assertEqual(res["fail"], 0, "\n".join(res["msgs"][:10]))
        self.assertGreater(res["ok"], 1000)
        print(f"\nSIM-17 größte Abweichung: {res['maxPos_in']:.6f} in, {res['maxHead_deg']:.6f}°")


if __name__ == "__main__":
    unittest.main()
