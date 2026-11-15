"""SIM-47: Die Läufe in tools/viewer/laeufe.json stehen wirklich im Workflow, der die Seite baut."""
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = (ROOT / ".github/workflows/dashboard.yml").read_text(encoding="utf-8")
RUNS = json.loads((ROOT / "tools/viewer/laeufe.json").read_text(encoding="utf-8"))["laeufe"]


class ViewerRunsTest(unittest.TestCase):
    def test_entries_are_complete_and_unique(self):
        self.assertGreaterEqual(len(RUNS), 2)
        for r in RUNS:
            self.assertTrue(r["id"] and r["name"] and r["dir"], r)
        self.assertEqual(len({r["id"] for r in RUNS}), len(RUNS))
        self.assertEqual(len({r["dir"] for r in RUNS}), len(RUNS))

    def test_workflow_writes_every_run_dir(self):
        for r in RUNS:
            self.assertIn(f"--out site/viewer/{r['dir']}", WORKFLOW, r["id"])

    def test_skills_run_uses_the_skills_routes(self):
        line = next(l for l in WORKFLOW.splitlines() if "--out site/viewer/lauf/skills" in l)
        for route in re.findall(r"routes/\S+\.route", line):
            self.assertTrue((ROOT / route).is_file(), route)
        self.assertIn("--stacker routes/skills_stacker.route", line)
        self.assertIn("--runner routes/skills_runner.route", line)

    def test_workflow_publishes_the_run_list(self):
        self.assertIn("tools/viewer/laeufe.json", WORKFLOW)


if __name__ == "__main__":
    unittest.main()
