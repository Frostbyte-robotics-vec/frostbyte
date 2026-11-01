"""Erzeugt künstliche Logs (Format docs/log_format.md) zum Testen des Viewers.

Kein Modell, keine echten Feldpunkte: Wegpunkte sind frei erfunden und nur für die Darstellung da.
Gleicher Seed = byte-gleiche Dateien.

    python3 tools/viewer/make_test_logs.py --out out/viewer_test/ [--seed 1]

Schreibt sim_stacker.csv, sim_runner.csv (source=sim) und robot_stacker.csv (source=robot, mit Drift).
"""
import argparse
import math
import random
from pathlib import Path

VERSION = 1
DT_S = 0.02
MATCH_S = 60.0
BUILD = "testlog"

COLUMNS = [
    "t_s", "robot", "x_in", "y_in", "h_deg", "gx_in", "gy_in", "gh_deg", "g_err_in",
    "tx_in", "ty_in", "th_deg", "lift_deg", "arm_deg", "claw_in", "lift_set_deg", "arm_set_deg",
    "cycle", "route_step", "has_cup", "has_pin", "pin_yellow_up", "score",
    "cpu_ms", "batt_v", "temp_max_c", "event",
]

# Erfundene Wegpunkte (Zoll, Ursprung Feldmitte) mit Namen für route_step. Keine Feldwahrheit.
WAYPOINTS = {
    "stacker": [("START", -48, -48), ("A", -36, -12), ("B", -12, 0), ("C", -36, 30), ("D", -54, 10), ("E", -20, -36)],
    "runner": [("START", 48, 48), ("A", 36, 12), ("B", 50, -24), ("C", 18, -46), ("D", 24, 30), ("E", 54, 2)],
}

# Liftwinkel unten/oben wie im Modell tools/viewer/reference/ (nur Darstellung, keine Hardwarewerte).
LIFT_DEG = {"stacker": (-40.0, 45.0), "runner": (-20.0, 60.0)}

# Phasen am Wegpunkt: (Zustand, Dauer s, Lift-Soll als Anteil unten→oben, Arm-Soll Grad, Greifer Zoll)
PHASES = [
    ("GREIFEN", 0.5, 0.0, 0, 0.0),
    ("HEBEN", 0.8, 1.0, 60, 0.0),
    ("AM_GOAL", 0.4, 1.0, 60, 0.0),
    ("SETZEN", 0.5, 0.85, 45, 2.0),
    ("RUECKZUG", 0.6, 0.0, 0, 2.0),
]
SPEED_IN_S = 30.0
TURN_DEG_S = 240.0


def fmt(v, nd=2):
    return "" if v is None else f"{v:.{nd}f}"


def wrap_deg(a):
    return (a + 180.0) % 360.0 - 180.0


def simulate(robot, rng):
    """Wahre Bahn: fährt Wegpunkte zyklisch ab, an jedem einen Ablege-Zyklus. Liefert Liste von Takten (dict)."""
    wps = WAYPOINTS[robot]
    x, y = wps[0][1], wps[0][2]
    h = 45.0 if robot == "stacker" else -135.0   # Grad, 0° = +x (rot → blau), gegen Uhrzeigersinn positiv
    lo, hi = LIFT_DEG[robot]
    lift, arm = lo, 0.0
    claw = 2.0
    wi, phase, phase_t = 1, None, 0.0
    score, has_cup, has_pin = 0, 0, 0
    steps = int(round(MATCH_S / DT_S))
    out = []
    for k in range(steps + 1):
        t = k * DT_S
        events = []
        name, wx, wy = wps[wi]
        lift_set, arm_set, claw_set = lo, 0.0, 2.0
        if phase is None:
            dx, dy = wx - x, wy - y
            dist = math.hypot(dx, dy)
            want = math.degrees(math.atan2(dy, dx))
            dh = wrap_deg(want - h)
            h = wrap_deg(h + max(-TURN_DEG_S * DT_S, min(TURN_DEG_S * DT_S, dh)))
            if abs(dh) < 30:
                step = min(dist, SPEED_IN_S * DT_S * (1 + 0.05 * rng.uniform(-1, 1)))
                x += step * math.cos(math.radians(h))
                y += step * math.sin(math.radians(h))
            cycle = "AUFNEHMEN" if wi % 2 else "BEREIT"
            if dist < 0.5:
                phase, phase_t = 0, 0.0
                has_cup = 1
        if phase is not None:
            cycle, dur, lift_f, arm_set, claw_set = PHASES[phase]
            lift_set = lo + lift_f * (hi - lo)
            if cycle == "GREIFEN" and robot == "stacker" and wi % 3 == 0:
                has_pin = 1
            phase_t += DT_S
            if phase_t >= dur - 1e-9:
                if cycle == "SETZEN":
                    ebene = 1 + (wi % 3)
                    events.append(f"PLACED:{name}:{ebene}")
                    score += 2 * ebene
                    has_cup = has_pin = 0
                phase += 1
                phase_t = 0.0
                if phase == len(PHASES):
                    phase = None
                    wi = wi % (len(wps) - 1) + 1
        if robot == "runner":
            arm_set = None
        lift += (lift_set - lift) * 0.12
        if arm_set is not None:
            arm += (arm_set - arm) * 0.15
        claw += (claw_set - claw) * 0.3
        if robot == "runner" and abs(t - 41.0) < 1e-9:
            events.append("BLOCKED")
        out.append(dict(
            t=t, x=x, y=y, h=h, lift=lift, arm=None if arm_set is None else arm, claw=claw,
            lift_set=lift_set, arm_set=arm_set, cycle=cycle,
            step=f"{wi}:cycle {name}" if phase is not None else f"{wi}:drive {name}",
            has_cup=has_cup, has_pin=has_pin, pin_yellow_up=has_pin, score=score, events=events,
        ))
    return out


def make_log(robot, source, seed):
    rng = random.Random(f"{robot}/{seed}")
    truth = simulate(robot, rng)
    is_sim = source == "sim"
    lines = [
        f"# frostbyte-log v{VERSION} robot={robot} source={source} seed={seed if is_sim else '-'} build={BUILD}",
        ",".join(COLUMNS),
    ]
    drx = dry = drh = 0.0
    batt, temp = 12.8, 28.0
    gps_gap = None
    for s in truth:
        # Odometrie: am Roboter mit Drift, in der Simulation fast wahr.
        drx += rng.gauss(0, 0.012 if not is_sim else 0.002)
        dry += rng.gauss(0, 0.012 if not is_sim else 0.002)
        drh += rng.gauss(0, 0.01 if not is_sim else 0.001)
        ox, oy, oh = s["x"] + drx, s["y"] + dry, wrap_deg(s["h"] + drh)
        # GPS: gelegentliche Ausfälle von 1 s.
        if gps_gap is None and rng.random() < 0.004:
            gps_gap = s["t"] + 1.0
        gps = gps_gap is None
        if gps_gap is not None and s["t"] >= gps_gap:
            gps_gap = None
        g_err = abs(rng.gauss(0.8, 0.3))
        events = list(s["events"])
        if not is_sim and abs(s["t"] - 23.5) < 1e-9:
            events.append("TIMEOUT:GREIFEN")
        batt -= 0.00002
        temp += 0.0004
        row = [
            fmt(s["t"]), robot, fmt(ox), fmt(oy), fmt(oh),
            fmt(s["x"] + rng.gauss(0, g_err) if gps else None),
            fmt(s["y"] + rng.gauss(0, g_err) if gps else None),
            fmt(wrap_deg(s["h"] + rng.gauss(0, 1.0)) if gps else None),
            fmt(g_err if gps else None),
            fmt(s["x"] if is_sim else None), fmt(s["y"] if is_sim else None), fmt(s["h"] if is_sim else None),
            fmt(s["lift"], 1), fmt(s["arm"], 1), fmt(s["claw"]), fmt(s["lift_set"], 1), fmt(s["arm_set"], 1),
            s["cycle"], s["step"], str(s["has_cup"]), str(s["has_pin"]), str(s["pin_yellow_up"]), str(s["score"]),
            fmt(rng.uniform(0.8, 2.6)), fmt(batt), fmt(temp, 1), ";".join(events),
        ]
        lines.append(",".join(row))
    return "\n".join(lines) + "\n"


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--out", default="out/viewer_test")
    ap.add_argument("--seed", type=int, default=1)
    a = ap.parse_args(argv)
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    for name, robot, source in (("sim_stacker", "stacker", "sim"), ("sim_runner", "runner", "sim"),
                                ("robot_stacker", "stacker", "robot")):
        (out / f"{name}.csv").write_text(make_log(robot, source, a.seed), encoding="utf-8", newline="")


if __name__ == "__main__":
    main()
