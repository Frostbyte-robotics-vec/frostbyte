"""Erzeugt die schlanke Simulationsseite (<Pages-URL>/sim/) aus dem Viewer (SIM-13).

Gleiche Datei wie der Viewer, nur mit Modus-Schalter und <base>, damit logparse.js, robots.js, Demo und field.json
aus /viewer/ geladen werden (kein doppelter Code).

Aufruf: python3 tools/sim_page.py tools/viewer/index.html site/sim/index.html
"""
import sys
from pathlib import Path


def build(src, dst):
    html = Path(src).read_text(encoding="utf-8")
    for anchor in ("<head>", "<body>"):
        if html.count(anchor) != 1:
            raise SystemExit(f"{src}: {anchor} muss genau einmal vorkommen")
    html = html.replace("<head>", '<head>\n<base href="../viewer/">', 1)
    html = html.replace("<body>", '<body data-ansicht="sim">', 1)
    Path(dst).parent.mkdir(parents=True, exist_ok=True)
    Path(dst).write_text(html, encoding="utf-8")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    build(sys.argv[1], sys.argv[2])
