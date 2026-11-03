#!/usr/bin/env python3
"""Route (.route) -> C++-Header für den Roboter. Dünner Aufruf von frostbyte_route_export (ADR-023):
Die Grammatik steht nur im C++-Parser (sim/route_parser.cpp), hier gibt es keinen zweiten.

  python3 tools/route2cpp.py routes/skills_stacker.route out/routes/skills_stacker.hpp
  [--symbol kRouteSkillsStacker] [--field field/field.json] [--exporter build-test/frostbyte_route_export]
Fehler in der Route: Meldung mit Zeilennummer, Exit 1.
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def default_symbol(route: Path) -> str:
    return "kRoute" + "".join(p.capitalize() for p in re.split(r"[^A-Za-z0-9]+", route.stem) if p)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("route")
    ap.add_argument("out")
    ap.add_argument("--symbol")
    ap.add_argument("--field", default=str(ROOT / "field" / "field.json"))
    ap.add_argument("--exporter", default=str(ROOT / "build-test" / "frostbyte_route_export"))
    a = ap.parse_args()
    exporter = Path(a.exporter)
    if not exporter.exists():
        print(f"{exporter} fehlt: cmake -S test -B build-test && cmake --build build-test", file=sys.stderr)
        return 2
    route = Path(a.route)
    Path(a.out).parent.mkdir(parents=True, exist_ok=True)
    p = subprocess.run([str(exporter), a.field, str(route), a.symbol or default_symbol(route), a.out])
    return p.returncode


if __name__ == "__main__":
    sys.exit(main())
