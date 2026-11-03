#!/usr/bin/env python3
"""Fasst die Läufe von frostbyte_sim zusammen (SIM-06).

  python3 tools/sim_report.py out/sim/ [--summary summary.json] [--html report.html]
                              [--stacker-route routes/skills_stacker.route] [--runner-route routes/skills_runner.route]

Liest DIR/run<NNN>_<roboter>.csv (Log-Format v1, docs/log_format.md), schreibt summary.json und einen HTML-Bericht
(P10/Median/P90 je Kennzahl und Roboter). Exit-Code 1 bei Regelverstoß (RULE:… im Log) oder wenn der P10 der Punkte unter
target_p10 der Route liegt. Fehlt der Punktestand im Log (Spielzustand: SIM-20, ohne Szenario oder Routenspieler A-03 gibt es keinen), wird target_p10 ausdrücklich als „nicht geprüft“
gemeldet, nicht als bestanden gezählt.
"""
import argparse
import csv
import glob
import html
import json
import math
import os
import re
import sys

METRICS = ["odo_err_max_in", "odo_err_final_in", "gps_valid_fraction", "wall_contacts", "robot_contacts", "score"]
LABELS = {
    "odo_err_max_in": "Odometriefehler max. (in)",
    "odo_err_final_in": "Odometriefehler am Ende (in)",
    "gps_valid_fraction": "GPS gültig (Anteil)",
    "wall_contacts": "Kontakte Feldrand",
    "robot_contacts": "Kontakte Roboter",
    "score": "Punkte",
}


def percentile(values, p):
    """Linear interpoliert; leere Liste -> None."""
    if not values:
        return None
    v = sorted(values)
    k = (len(v) - 1) * p / 100.0
    lo, hi = math.floor(k), math.ceil(k)
    return v[lo] if lo == hi else v[lo] + (v[hi] - v[lo]) * (k - lo)


def _num(cell):
    return float(cell) if cell not in ("", None) else None


def run_metrics(path):
    errs, rows, with_truth, gps_valid = [], 0, 0, 0
    wall = robot = rules = 0
    timeouts = {}
    score = None
    with open(path, encoding="utf-8", newline="") as f:
        lines = [ln for ln in f if not ln.startswith("#")]
    for r in csv.DictReader(lines):
        rows += 1
        tx, ty = _num(r["tx_in"]), _num(r["ty_in"])
        if tx is not None and ty is not None:
            with_truth += 1
            if _num(r["gx_in"]) is not None:
                gps_valid += 1
            x, y = _num(r["x_in"]), _num(r["y_in"])
            if x is not None and y is not None:
                errs.append(math.hypot(x - tx, y - ty))
        if _num(r["score"]) is not None:
            score = _num(r["score"])
            score = int(score) if score == int(score) else score
        for ev in filter(None, r["event"].split(";")):
            if ev == "COLLISION:wall":
                wall += 1
            elif ev.startswith("COLLISION:"):
                robot += 1
            elif ev.startswith("RULE:"):
                rules += 1
            elif ev.startswith("TIMEOUT:"):
                state = ev[len("TIMEOUT:"):]
                timeouts[state] = timeouts.get(state, 0) + 1
    return {
        "rows": rows,
        "odo_err_max_in": max(errs) if errs else None,
        "odo_err_final_in": errs[-1] if errs else None,
        "gps_valid_fraction": gps_valid / with_truth if with_truth else None,
        "wall_contacts": wall,
        "robot_contacts": robot,
        "rule_violations": rules,
        "timeouts": timeouts,
        "score": score,
    }


def summarize(directory):
    files = sorted(glob.glob(os.path.join(directory, "run*_*.csv")))
    pat = re.compile(r"run(\d+)_(\w+)\.csv$")
    per_robot, run_ids = {}, set()
    for p in files:
        m = pat.search(os.path.basename(p))
        if m:
            run_ids.add(m.group(1))
            per_robot.setdefault(m.group(2), []).append(run_metrics(p))
    if not per_robot:
        sys.exit(f"Keine Läufe (run<NNN>_<roboter>.csv) in {directory}")
    robots, violations = {}, 0
    for name, runs in per_robot.items():
        robots[name] = {}
        for k in METRICS:
            vals = [r[k] for r in runs if r[k] is not None]
            robots[name][k] = ({"p10": percentile(vals, 10), "median": percentile(vals, 50), "p90": percentile(vals, 90)}
                               if vals else None)
        failures = {}
        for r in runs:
            for state, n in r["timeouts"].items():
                f = failures.setdefault(state, {"total": 0, "runs": 0})
                f["total"] += n
                f["runs"] += 1
        robots[name]["failures_by_state"] = dict(sorted(failures.items()))
        violations += sum(r["rule_violations"] for r in runs)
    return {"runs": len(run_ids), "robots": robots, "rule_violations": violations}


def route_target(path):
    """target_p10 aus einer Routendatei; None, wenn Datei oder Zeile fehlt."""
    try:
        with open(path, encoding="utf-8") as f:
            for line in f:
                m = re.match(r"\s*target_p10\s+([0-9.]+)", line)
                if m:
                    return float(m.group(1))
    except OSError:
        return None
    return None


def acceptance(summary, targets):
    ok, msgs = True, []
    if summary["rule_violations"]:
        ok = False
        msgs.append(f"Regelverstoß (RULE:…) in {summary['rule_violations']} Fällen")
    for robot, target in targets.items():
        score = summary["robots"].get(robot, {}).get("score")
        if score is None:
            msgs.append(f"target_p10 {target:g} für {robot} nicht geprüft (kein Punktestand im Log: Spielzustand nötig, SIM-20)")
        elif score["p10"] < target:
            ok = False
            msgs.append(f"{robot}: P10 der Punkte {score['p10']:g} liegt unter target_p10 {target:g}")
    return ok, msgs


def _fmt(v):
    return "–" if v is None else f"{v:.3g}"


def html_report(summary, msgs):
    e = html.escape
    out = ["<!DOCTYPE html><html lang=\"de\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">",
           "<title>Simulationsbericht</title><style>body{font:15px system-ui,sans-serif;margin:24px;max-width:960px;color:#13202b;background:#ecf0f3}"
           "table{border-collapse:collapse;width:100%;background:#fff;margin:12px 0}th,td{border:1px solid #c7d1d8;padding:6px 10px;text-align:right}"
           "th:first-child,td:first-child{text-align:left}.bad{color:#b3261e}.ok{color:#2f7d4f}"
           "@media(prefers-color-scheme:dark){body{background:#0d141a;color:#e2eaf0}table{background:#141e26}th,td{border-color:#26343f}}"
           "</style></head><body>",
           f"<h1>Simulationsbericht</h1><p>{summary['runs']} Läufe · Regelverstöße: "
           f"<span class=\"{'bad' if summary['rule_violations'] else 'ok'}\">{summary['rule_violations']}</span></p>"]
    for m in msgs:
        out.append(f"<p>{e(m)}</p>")
    for robot, mets in sorted(summary["robots"].items()):
        out.append(f"<h2>{e(robot)}</h2><table><tr><th>Kennzahl</th><th>P10</th><th>Median</th><th>P90</th></tr>")
        for k in METRICS:
            v = mets[k]
            out.append(f"<tr><td>{e(LABELS[k])}</td><td>{_fmt(v and v['p10'])}</td><td>{_fmt(v and v['median'])}</td>"
                       f"<td>{_fmt(v and v['p90'])}</td></tr>")
        out.append("</table>")
        fails = mets["failures_by_state"]
        if fails:
            out.append("<table><tr><th>Fehler (Timeout) je Zustand</th><th>Anzahl</th><th>Läufe betroffen</th></tr>")
            for state, f in fails.items():
                out.append(f"<tr><td>{e(state)}</td><td>{f['total']}</td><td>{f['runs']}</td></tr>")
            out.append("</table>")
        else:
            out.append("<p>Keine Timeouts in den Läufen.</p>")
    if any(m.get("score") is None for m in summary["robots"].values()):
        out.append("<p>Punkte: „–“, kein Punktestand im Log (der Spielzustand liefert ihn, SIM-20; ohne Szenario oder "
                   "Routenspieler A-03 bleibt die Spalte leer).</p>")
    out.append("</body></html>")
    return "".join(out)


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("dir")
    ap.add_argument("--summary")
    ap.add_argument("--html")
    ap.add_argument("--stacker-route")
    ap.add_argument("--runner-route")
    a = ap.parse_args(argv)
    summary = summarize(a.dir)
    targets = {}
    for robot, path in (("stacker", a.stacker_route), ("runner", a.runner_route)):
        t = route_target(path) if path else None
        if t is not None:
            targets[robot] = t
    ok, msgs = acceptance(summary, targets)
    with open(a.summary or os.path.join(a.dir, "summary.json"), "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2, ensure_ascii=False)
    with open(a.html or os.path.join(a.dir, "report.html"), "w", encoding="utf-8") as f:
        f.write(html_report(summary, msgs))
    for m in msgs:
        print(m)
    print(f"{summary['runs']} Läufe, Regelverstöße {summary['rule_violations']}: {'OK' if ok else 'FEHLER'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
