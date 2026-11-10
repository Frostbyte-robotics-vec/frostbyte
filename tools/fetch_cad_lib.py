#!/usr/bin/env python3
"""Holt die VEX-Teilebibliothek (STEP, aus CAD-02) aus dem GitHub-Release nach cad/lib/ (nicht im Git).

Aufruf: python3 tools/fetch_cad_lib.py
Läuft beim ersten Mal mit Download (ca. 117 MB), danach tut es nichts mehr (Marker-Datei in cad/lib/).
Neue Fassung der Bibliothek: URL und SHA256 unten ändern, cad/lib/ löschen.
"""
import hashlib
import shutil
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path

URL = "https://github.com/Frostbyte-robotics-vec/frostbyte/releases/download/vex-step-1.0.0/vex_step_1_0_0.zip"
SHA256 = "06cc8553f5e91475876768e8d5b006e2a2ea7c4a4a3f8057b93f6657842d7246"
DEST = Path(__file__).resolve().parent.parent / "cad" / "lib"
MARKER = ".frostbyte_fetched"


class FetchError(Exception):
    pass


def _sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def _extract(zip_path, dest):
    dest = dest.resolve()
    with zipfile.ZipFile(zip_path) as z:
        for name in z.namelist():
            if not (dest / name).resolve().is_relative_to(dest):
                raise FetchError("unsicherer Pfad im ZIP: " + name)
        z.extractall(dest)


def fetch(url, sha256, dest):
    """True, wenn geladen wurde; False, wenn die Bibliothek schon da ist."""
    dest = Path(dest)
    if (dest / MARKER).exists():
        return False
    with tempfile.TemporaryDirectory() as tmp:
        zip_path = Path(tmp) / "lib.zip"
        try:
            with urllib.request.urlopen(url, timeout=60) as r, open(zip_path, "wb") as f:
                shutil.copyfileobj(r, f)
        except OSError as e:
            raise FetchError("Download fehlgeschlagen: %s" % e)
        if _sha256(zip_path) != sha256:
            raise FetchError("Prüfsumme stimmt nicht, nichts entpackt")
        staging = Path(tmp) / "out"
        _extract(zip_path, staging)
        (staging / MARKER).write_text(sha256 + "\n")
        dest.parent.mkdir(parents=True, exist_ok=True)
        if dest.exists():
            shutil.rmtree(dest)
        shutil.move(str(staging), str(dest))
    return True


def main():
    if (DEST / MARKER).exists():
        print("cad/lib: VEX-STEP-Bibliothek schon vorhanden")
        return 0
    print("cad/lib: lade VEX-STEP-Bibliothek (ca. 117 MB) ...")
    try:
        fetch(URL, SHA256, DEST)
    except FetchError as e:
        print("cad/lib: " + str(e), file=sys.stderr)
        return 1
    print("cad/lib: fertig")
    return 0


if __name__ == "__main__":
    sys.exit(main())
