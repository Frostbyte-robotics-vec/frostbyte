import hashlib
import ssl
import sys
import tempfile
import unittest
import urllib.error
import zipfile
from pathlib import Path
from unittest import mock

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

    def test_zertifikatsfehler_faellt_auf_curl_zurueck(self):
        # python.org-Python ohne Zertifikate (macOS): curl nutzt den System-Speicher; Prüfsumme gilt weiter
        sha = make_zip(self.zip, {"a.step": "A"})
        err = urllib.error.URLError(ssl.SSLCertVerificationError("unable to get local issuer certificate"))
        calls = []

        def fake_curl(cmd, **kw):
            calls.append(cmd)
            Path(cmd[cmd.index("-o") + 1]).write_bytes(self.zip.read_bytes())
            return mock.Mock(returncode=0)

        with mock.patch("urllib.request.urlopen", side_effect=err), \
                mock.patch("shutil.which", return_value="/usr/bin/curl"), mock.patch("subprocess.run", side_effect=fake_curl):
            self.assertTrue(fetch_cad_lib.fetch("https://example.invalid/lib.zip", sha, self.dest))
        self.assertEqual(calls[0][0], "/usr/bin/curl")
        self.assertEqual((self.dest / "a.step").read_text(), "A")

    def test_zertifikatsfehler_ohne_curl_bleibt_sauberer_fehler(self):
        err = urllib.error.URLError(ssl.SSLCertVerificationError("x"))
        with mock.patch("urllib.request.urlopen", side_effect=err), mock.patch("shutil.which", return_value=None):
            with self.assertRaises(fetch_cad_lib.FetchError):
                fetch_cad_lib.fetch("https://example.invalid/lib.zip", "0" * 64, self.dest)
        self.assertFalse(self.dest.exists())

    def test_curl_fehler_bleibt_sauberer_fehler(self):
        err = urllib.error.URLError(ssl.SSLCertVerificationError("x"))
        with mock.patch("urllib.request.urlopen", side_effect=err), mock.patch("shutil.which", return_value="/usr/bin/curl"), \
                mock.patch("subprocess.run", return_value=mock.Mock(returncode=22)):
            with self.assertRaises(fetch_cad_lib.FetchError):
                fetch_cad_lib.fetch("https://example.invalid/lib.zip", "0" * 64, self.dest)

    def test_konstanten_gesetzt(self):
        self.assertTrue(fetch_cad_lib.URL.startswith("https://github.com/"))
        self.assertEqual(len(fetch_cad_lib.SHA256), 64)


if __name__ == "__main__":
    unittest.main()
