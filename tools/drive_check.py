#!/usr/bin/env python3
"""Wertet eine Antriebs-Prüfung aus (D-07): 2 m geradeaus oder 10 Drehungen.

  python3 tools/drive_check.py straight <log.csv> [--measured-cm CM] [--route routes/drive_straight.route]
  python3 tools/drive_check.py turns    <log.csv> [--measured-deg GRAD] [--route routes/drive_turns.route]

Soll-Ende: letzte `goto pose` bzw. `turn` der Route. Istwert: in der Simulation die wahre Pose (tx_in, ty_in, th_deg) der
letzten Logzeile. Am Roboter gibt es keine wahre Pose: dort misst ein Mensch (Maßband, Winkelmesser) und übergibt den Wert mit
--measured-*; die Odometrie wird nur zum Vergleich ausgegeben, denn sie glaubt denselben Sensoren wie der Regler.
Grenzen (docs/plan.md, D-07): geradeaus <= 2 cm Abstand zum Ziel, nach 10 Drehungen <= 2 Grad Richtungsfehler.
Ausgang: 0 bestanden, 1 nicht bestanden, 2 Eingabefehler.
"""
import argparse
import math
import os
import re
import sys
from dataclasses import dataclass
from typing import Optional

CM_PER_IN = 2.54
LIMITS = {"straight": 2.0, "turns": 2.0}
UNITS = {"straight": "cm", "turns": "deg"}
DEFAULT_ROUTE = {"straight": "routes/drive_straight.route", "turns": "routes/drive_turns.route"}


class CheckError(Exception):
    pass


@dataclass
class Result:
    passed: bool
    value: float  # Abweichung in cm bzw. Grad
    limit: float
    unit: str
    source: str  # "wahre Pose" oder "gemessen"
    odometry: Optional[float]  # gleiche Abweichung aus der Odometrie, nur zum Vergleich


def _target(kind, route_path):
    """Soll-Ende aus der Route: (x, y) der letzten goto pose bzw. Richtung der letzten turn."""
    last = None
    try:
        lines = open(route_path, encoding="utf-8").read().splitlines()
    except OSError as e:
        raise CheckError(f"Route nicht lesbar: {route_path}: {e}")
    for line in lines:
        line = line.split("#", 1)[0].strip()
        if kind == "straight":
            m = re.match(r"goto\s+pose\s+.*", line)
            if m:
                x = re.search(r"\bx=(-?[\d.]+)", line)
                y = re.search(r"\by=(-?[\d.]+)", line)
                if x and y:
                    last = (float(x.group(1)), float(y.group(1)))
        else:
            m = re.match(r"turn\s+(-?[\d.]+)", line)
            if m:
                last = float(m.group(1))
    if last is None:
        raise CheckError(f"Route {route_path} enthält keine {'goto pose' if kind == 'straight' else 'turn'}-Anweisung")
    return last


def _last_row(log_path):
    try:
        lines = open(log_path, encoding="utf-8").read().splitlines()
    except OSError as e:
        raise CheckError(f"Log nicht lesbar: {log_path}: {e}")
    if len(lines) < 3 or not lines[0].startswith("# frostbyte-log"):
        raise CheckError("Log ohne Kopfzeile '# frostbyte-log' oder ohne Datenzeilen")
    cols = lines[1].split(",")
    cells = lines[-1].split(",")
    return dict(zip(cols, cells))


def _num(row, name):
    v = row.get(name, "")
    try:
        return float(v) if v != "" else None
    except ValueError:
        return None


def _wrap(deg):
    return (deg + 180.0) % 360.0 - 180.0


def check(kind, log_path, route_path, measured=None):
    if kind not in LIMITS:
        raise CheckError(f"Unbekannte Prüfung: {kind}")
    target = _target(kind, route_path)
    row = _last_row(log_path)

    def deviation(x, y, h):
        if kind == "straight":
            return None if x is None or y is None else math.hypot(x - target[0], y - target[1]) * CM_PER_IN
        return None if h is None else abs(_wrap(h - target))

    truth = deviation(_num(row, "tx_in"), _num(row, "ty_in"), _num(row, "th_deg"))
    odo = deviation(_num(row, "x_in"), _num(row, "y_in"), _num(row, "h_deg"))
    if measured is not None:
        value, source = measured, "gemessen"
    elif truth is not None:
        value, source = truth, "wahre Pose"
    else:
        raise CheckError("Keine wahre Pose im Log (Roboter-Log): Abweichung messen und mit "
                         f"--measured-{UNITS[kind]} übergeben")
    return Result(value <= LIMITS[kind] + 1e-9, value, LIMITS[kind], UNITS[kind], source, odo)


def main(argv=None):
    ap = argparse.ArgumentParser(description="Antriebs-Prüfung auswerten (D-07)")
    ap.add_argument("kind", choices=sorted(LIMITS))
    ap.add_argument("log")
    ap.add_argument("--route", help="Route der Prüfung (Standard: routes/drive_*.route)")
    ap.add_argument("--measured-cm", type=float, help="gemessener Abstand zum Ziel in cm (nur straight)")
    ap.add_argument("--measured-deg", type=float, help="gemessener Richtungsfehler in Grad (nur turns)")
    args = ap.parse_args(argv)
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    route = args.route or os.path.join(root, DEFAULT_ROUTE[args.kind])
    measured = args.measured_cm if args.kind == "straight" else args.measured_deg
    if (args.kind == "straight" and args.measured_deg is not None) or (args.kind == "turns" and args.measured_cm is not None):
        print("Fehler: falsche Einheit für diese Prüfung", file=sys.stderr)
        return 2
    if measured is not None and measured < 0:
        print("Fehler: gemessener Wert muss >= 0 sein (Betrag der Abweichung)", file=sys.stderr)
        return 2
    try:
        r = check(args.kind, args.log, route, measured)
    except CheckError as e:
        print(f"Fehler: {e}", file=sys.stderr)
        return 2
    odo = f", Odometrie {r.odometry:.2f} {r.unit}" if r.odometry is not None else ""
    print(f"{'bestanden' if r.passed else 'nicht bestanden'}: Abweichung {r.value:.2f} {r.unit} "
          f"(Grenze {r.limit:.1f} {r.unit}, {r.source}{odo})")
    return 0 if r.passed else 1


if __name__ == "__main__":
    sys.exit(main())
