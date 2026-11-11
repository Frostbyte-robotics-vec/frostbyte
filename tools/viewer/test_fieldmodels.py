"""Führt die JS-Tests der Modellauswahl (fieldmodels_test.js) unter Node aus (SIM-37).

Ausführen: python3 -m unittest discover -s tools/viewer -p "test_fieldmodels.py"
Ohne node wird übersprungen; in der CI ist das ein Fehler. Dann im Browser tools/viewer/index.html?selftest öffnen.
"""
import json
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent


class TestFieldmodels(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not shutil.which("node"):
            if os.environ.get("CI"):
                raise AssertionError("node fehlt in der CI")
            raise unittest.SkipTest("node fehlt")

    def test_js_einheitentests(self):
        src = (HERE / "fieldmodels.js").read_text(encoding="utf-8") + "\n" + \
            (HERE / "fieldmodels_test.js").read_text(encoding="utf-8") + \
            "\nconsole.log(JSON.stringify(runFieldmodelsTests(FBModels)));"
        with tempfile.NamedTemporaryFile("w", suffix=".js", delete=False, encoding="utf-8") as f:
            f.write(src)
        try:
            r = subprocess.run(["node", f.name], capture_output=True, text=True, timeout=60, encoding="utf-8")
        finally:
            Path(f.name).unlink()
        self.assertEqual(r.returncode, 0, r.stderr)
        res = json.loads(r.stdout)
        self.assertEqual(res["fail"], 0, "\n".join(res["msgs"]))
        self.assertGreater(res["ok"], 25)


if __name__ == "__main__":
    unittest.main()
