import hashlib
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fetch_cad_lib  # noqa: E402


def make_zip(path, files):
    with zipfile.ZipFile(path, "w") as z:
        for name, text in files.items():
            z.writestr(name, text)
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


class FetchCadLibTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self.tmp.name)
        self.zip = self.dir / "lib.zip"
        self.dest = self.dir / "cad" / "lib"

    def tearDown(self):
        self.tmp.cleanup()

    def test_erstlauf_laedt_und_entpackt(self):
        sha = make_zip(self.zip, {"Motion/Gears/a.step": "A", "Electronics/b.step": "B"})
        self.assertTrue(fetch_cad_lib.fetch(self.zip.as_uri(), sha, self.dest))
        self.assertEqual((self.dest / "Motion/Gears/a.step").read_text(), "A")
        self.assertEqual((self.dest / "Electronics/b.step").read_text(), "B")

    def test_zweiter_lauf_laedt_nicht_erneut(self):
        sha = make_zip(self.zip, {"a.step": "A"})
        fetch_cad_lib.fetch(self.zip.as_uri(), sha, self.dest)
        self.zip.unlink()  # waere ein Download noetig, schluege er jetzt fehl
        self.assertFalse(fetch_cad_lib.fetch(self.zip.as_uri(), sha, self.dest))

    def test_falsche_pruefsumme_entpackt_nichts(self):
        make_zip(self.zip, {"a.step": "A"})
        with self.assertRaises(fetch_cad_lib.FetchError):
            fetch_cad_lib.fetch(self.zip.as_uri(), "0" * 64, self.dest)
        self.assertFalse(self.dest.exists())

    def test_unsicherer_zip_pfad_wird_abgelehnt(self):
        sha = make_zip(self.zip, {"../boese.txt": "x"})
        with self.assertRaises(fetch_cad_lib.FetchError):
            fetch_cad_lib.fetch(self.zip.as_uri(), sha, self.dest)
        self.assertFalse((self.dir / "cad" / "boese.txt").exists())

    def test_fehlgeschlagener_lauf_laesst_neuen_versuch_zu(self):
        make_zip(self.zip, {"a.step": "A"})
        with self.assertRaises(fetch_cad_lib.FetchError):
            fetch_cad_lib.fetch(self.zip.as_uri(), "0" * 64, self.dest)
        sha = hashlib.sha256(self.zip.read_bytes()).hexdigest()
        self.assertTrue(fetch_cad_lib.fetch(self.zip.as_uri(), sha, self.dest))

    def test_konstanten_gesetzt(self):
        self.assertTrue(fetch_cad_lib.URL.startswith("https://github.com/"))
        self.assertEqual(len(fetch_cad_lib.SHA256), 64)


if __name__ == "__main__":
    unittest.main()
