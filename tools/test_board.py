"""Tests für tools/board.py (S-05: Status nach dem Merge eines PRs).

Aufruf: python3 -m unittest discover -s tools -p "test_board.py"
Schreibt nie ins echte tasks/, sondern in ein temporäres Verzeichnis.
"""
import contextlib, io, pathlib, sys, tempfile, unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import board  # noqa: E402

TICKET = """---
id: X-01
phase: "0"
titel: Testticket
status: {status}
owner: {owner}
branch: ticket/x-01
depends: []
hw: nein
---

## Ziel
Test
"""


class StatusAfterMerge(unittest.TestCase):
    def test_review_wird_done(self):
        self.assertEqual(board.status_after_merge("review"), "done")

    def test_needs_hw_bleibt(self):
        self.assertEqual(board.status_after_merge("needs_hw"), "needs_hw")

    def test_done_bleibt_done(self):
        self.assertEqual(board.status_after_merge("done"), "done")

    def test_uebrige_wie_bisher_done(self):
        for s in ("todo", "in_progress", "blocked"):
            with self.subTest(status=s):
                self.assertEqual(board.status_after_merge(s), "done")

    def test_ergebnis_ist_bekannter_status(self):
        for s in board.STATUS:
            with self.subTest(status=s):
                self.assertIn(board.status_after_merge(s), board.STATUS)


class MergedBefehl(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.tasks = pathlib.Path(self._tmp.name)
        self._orig = board.TASKS
        board.TASKS = self.tasks

    def tearDown(self):
        board.TASKS = self._orig
        self._tmp.cleanup()

    def run_merged(self, status, owner="jemand"):
        (self.tasks / "X-01.md").write_text(TICKET.format(status=status, owner=owner), encoding="utf-8")
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            rc = board.main(["merged", "X-01"])
        return rc, board.load()["X-01"]["status"], out.getvalue()

    def test_review_wird_in_datei_done(self):
        rc, status, _ = self.run_merged("review")
        self.assertEqual(rc, 0)
        self.assertEqual(status, "done")

    def test_needs_hw_bleibt_in_datei(self):
        rc, status, out = self.run_merged("needs_hw")
        self.assertEqual(rc, 0)
        self.assertEqual(status, "needs_hw")
        self.assertIn("needs_hw", out)

    def test_board_bleibt_gueltig(self):
        for s in ("review", "needs_hw"):
            with self.subTest(status=s):
                self.run_merged(s)
                with contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(board.cmd_validate(board.load()), 0)

    def test_unbekanntes_ticket(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            rc = board.main(["merged", "GIBT-ES-NICHT"])
        self.assertEqual(rc, 1)


if __name__ == "__main__":
    unittest.main()
