#!/usr/bin/env python3
"""Formatiert alle C++-Dateien mit clang-format.

  python3 tools/format.py          formatiert in place
  python3 tools/format.py --check  ändert nichts, Exit 1 bei Abweichung (CI)
"""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DIRS = ["src", "include", "lib", "hal", "config", "test", "sim", "tools"]
EXTS = {".cpp", ".hpp", ".h", ".cc"}
SKIP = {"build", "build-test", "out", "site", "firmware", "third_party"}


def files():
    for d in DIRS:
        base = ROOT / d
        if not base.is_dir():
            continue
        for p in sorted(base.rglob("*")):
            if p.suffix in EXTS and not SKIP.intersection(p.relative_to(ROOT).parts):
                yield p


def main():
    check = "--check" in sys.argv[1:]
    fs = [str(p) for p in files()]
    if not fs:
        print("keine C++-Dateien, nichts zu tun")
        return 0
    cmd = ["clang-format", "--style=file"] + (["--dry-run", "--Werror"] if check else ["-i"]) + fs
    try:
        return subprocess.call(cmd, cwd=ROOT)
    except FileNotFoundError:
        print("clang-format nicht gefunden (pip install clang-format)", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
