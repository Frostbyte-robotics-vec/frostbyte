#!/usr/bin/env python3
"""Zeichnet ein Log (Format v1, docs/log_format.md) von Roboter oder Simulation (F-05).

  python3 tools/log_plot.py DATEI.csv [--html plot.html]

Schreibt eine eigenständige HTML-Seite mit SVG-Diagrammen: Bahn (x/y), je ein Verlauf über t_s für jede Spalte, die
Werte enthält (cpu_ms, batt_v, lift_deg, ...), und eine Liste der Ereignisse. Nur Standardbibliothek. Unbekannte
Ereignis-Token werden angezeigt, nicht abgelehnt (Leser müssen sie ignorieren können). Exit-Code 1 bei ungültiger Datei.
"""
import argparse
import csv
import html
import math
import sys

COLUMNS = (
    "t_s,robot,x_in,y_in,h_deg,gx_in,gy_in,gh_deg,g_err_in,tx_in,ty_in,th_deg,lift_deg,arm_deg,claw_in,"
    "lift_set_deg,arm_set_deg,cycle,route_step,has_cup,has_pin,pin_yellow_up,score,cpu_ms,batt_v,temp_max_c,event"
).split(",")
SERIES = ["cpu_ms", "batt_v", "temp_max_c", "lift_deg", "lift_set_deg", "arm_deg", "arm_set_deg", "claw_in", "score",
          "g_err_in", "h_deg"]


class LogError(Exception):
    pass


def read_log(path):
    """Gibt (Kopf-Dict, Zeilen als Liste von Dicts) zurück. Leere Zelle -> None, Zahlen -> float."""
    with open(path, newline="", encoding="utf-8") as f:
        first = f.readline().rstrip("\r\n")
        if not first.startswith("# frostbyte-log v1 "):
            raise LogError("Kopfzeile fehlt oder ist nicht Log v1: %r" % first[:60])
        head = dict(p.split("=", 1) for p in first.split()[3:] if "=" in p)
        reader = csv.reader(f)
        names = next(reader, None)
        if names != COLUMNS:
            raise LogError("Spaltenzeile weicht von docs/log_format.md ab")
        rows = []
        for n, cells in enumerate(reader, start=3):
            if len(cells) != len(COLUMNS):
                raise LogError("Zeile %d: %d statt %d Spalten" % (n, len(cells), len(COLUMNS)))
            row = {}
            for name, cell in zip(COLUMNS, cells):
                if cell == "":
                    row[name] = None
                elif name in ("robot", "cycle", "route_step", "event"):
                    row[name] = cell
                else:
                    try:
                        row[name] = float(cell)
                    except ValueError:
                        raise LogError("Zeile %d, Spalte %s: keine Zahl: %r" % (n, name, cell))
            rows.append(row)
    return head, rows


def events(rows):
    """Alle Ereignisse als (t_s, token); mehrere je Zeile sind mit ';' getrennt."""
    out = []
    for r in rows:
        if r["event"]:
            for tok in r["event"].split(";"):
                if tok:
                    out.append((r["t_s"], tok))
    return out


def _svg_line(points, w=640, h=140, title="", unit=""):
    pts = [(x, y) for x, y in points if y is not None and not math.isnan(y)]
    if len(pts) < 2:
        return ""
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    x0, x1 = min(xs), max(xs)
    y0, y1 = min(ys), max(ys)
    if x1 == x0:
        x1 = x0 + 1
    if y1 == y0:
        y1 = y0 + 1
    pad = 34

    def sx(x):
        return pad + (x - x0) / (x1 - x0) * (w - pad - 8)

    def sy(y):
        return h - 18 - (y - y0) / (y1 - y0) * (h - 32)

    d = " ".join("%.1f,%.1f" % (sx(x), sy(y)) for x, y in pts)
    return (
        '<figure><figcaption>%s <span class="u">%s · %.3g … %.3g</span></figcaption>'
        '<svg viewBox="0 0 %d %d" role="img" aria-label="%s"><polyline fill="none" stroke="currentColor" '
        'stroke-width="1.5" points="%s"/><text x="%d" y="%d" class="ax">%.3g s</text>'
        '<text x="%d" y="%d" class="ax" text-anchor="end">%.3g s</text></svg></figure>'
        % (html.escape(title), html.escape(unit), y0, y1, w, h, html.escape(title), d, pad, h - 4, x0, w - 8, h - 4, x1)
    )


def _svg_path(rows, w=360, h=360):
    pts = [(r["x_in"], r["y_in"]) for r in rows if r["x_in"] is not None and r["y_in"] is not None]
    if len(pts) < 2:
        return ""
    lim = max(72.0, max(abs(v) for p in pts for v in p))

    def sx(x):
        return w / 2 + x / lim * (w / 2 - 6)

    def sy(y):
        return h / 2 - y / lim * (h / 2 - 6)

    d = " ".join("%.1f,%.1f" % (sx(x), sy(y)) for x, y in pts)
    return (
        '<figure><figcaption>Bahn <span class="u">Zoll, +x = rot→blau</span></figcaption>'
        '<svg viewBox="0 0 %d %d" role="img" aria-label="Bahn"><rect x="3" y="3" width="%d" height="%d" fill="none" '
        'stroke="currentColor" opacity=".3"/><polyline fill="none" stroke="currentColor" stroke-width="1.5" '
        'points="%s"/></svg></figure>' % (w, h, w - 6, h - 6, d)
    )


def build_html(path, head, rows):
    parts = []
    parts.append(_svg_path(rows))
    for name in SERIES:
        parts.append(_svg_line([(r["t_s"], r[name]) for r in rows], title=name))
    evs = events(rows)
    ev_html = "".join("<li><code>%.3f s</code> %s</li>" % (t, html.escape(tok)) for t, tok in evs[:500])
    if len(evs) > 500:
        ev_html += "<li>… %d weitere</li>" % (len(evs) - 500)
    t_end = rows[-1]["t_s"] if rows else 0.0
    return (
        '<!doctype html><html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width">'
        "<title>Log %s</title><style>:root{color-scheme:light dark}body{font:15px system-ui;margin:16px auto;"
        "max-width:700px;padding:0 16px}figure{margin:12px 0}figcaption{font-weight:600}.u,.ax{font-weight:400;"
        "opacity:.7;font-size:11px}svg{width:100%%;height:auto}</style></head><body><h1>%s</h1>"
        "<p>%d Zeilen, %.1f s. Kopf: %s</p>%s<h2>Ereignisse (%d)</h2><ul>%s</ul></body></html>"
        % (
            html.escape(path),
            html.escape(path),
            len(rows),
            t_end,
            html.escape(" ".join("%s=%s" % kv for kv in head.items())),
            "".join(parts),
            len(evs),
            ev_html,
        )
    )


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("log")
    ap.add_argument("--html", default=None, help="Ausgabe (Standard: <log>.html)")
    args = ap.parse_args(argv)
    try:
        head, rows = read_log(args.log)
    except (OSError, LogError) as e:
        print("log_plot: %s" % e, file=sys.stderr)
        return 1
    out = args.html or args.log.rsplit(".", 1)[0] + ".html"
    with open(out, "w", encoding="utf-8") as f:
        f.write(build_html(args.log, head, rows))
    print("%d Zeilen -> %s" % (len(rows), out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
