import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import route2cpp  # noqa: E402

ROOT = route2cpp.ROOT
EXPORTER = route2cpp.find_exporter()


@unittest.skipUnless(EXPORTER.exists(), "frostbyte_route_export nicht gebaut (cmake -S test -B build-test)")
class Route2Cpp(unittest.TestCase):
    def run_tool(self, text, tmp):
        route = Path(tmp) / "x.route"
        route.write_text(text)
        out = Path(tmp) / "x.hpp"
        p = subprocess.run([sys.executable, str(ROOT / "tools" / "route2cpp.py"), str(route), str(out)],
                           capture_output=True, text=True)
        return p, out

    def test_gueltige_route_erzeugt_header(self):
        with tempfile.TemporaryDirectory() as tmp:
            p, out = self.run_tool("start START_STACKER\nwait 1\nend hold\n", tmp)
            self.assertEqual(p.returncode, 0, p.stderr)
            self.assertIn("kRouteX", out.read_text())

    def test_fehler_nennt_zeilennummer_und_schreibt_nichts(self):
        with tempfile.TemporaryDirectory() as tmp:
            p, out = self.run_tool("start START_STACKER\nfly\nend hold\n", tmp)
            self.assertEqual(p.returncode, 1)
            self.assertIn("Zeile 2:", p.stderr)
            self.assertFalse(out.exists())


if __name__ == "__main__":
    unittest.main()
