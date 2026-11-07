#!/usr/bin/env python3
"""Wertet das Protokoll des Funk-Link-Tests aus (L-01, ADR-041).

  python3 tools/link_report.py link.csv [--max-loss 0.02] [--max-p95-ms 40] [--json out.json]

Liest ein frostbyte-linklog v1 (Kopfzeile, dann seq,t_send_s,rtt_ms; leere rtt-Zelle = verloren) und gibt Anzahl, Verlustrate,
längste Verlustserie sowie Latenz (min, Median, P95, max) aus. Die Latenz ist die Umlaufzeit (RTT) in ms.
Ohne Grenzwerte nur Bericht (Exit 0). Mit Grenzwerten Exit 1, wenn einer überschritten ist oder nichts gemessen wurde.
Die Grenzwerte stehen bewusst nicht im Skript: sie kommen aus der Messung am Roboter (TODO(HW), L-04).
"""
import argparse
import csv
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sim_report import percentile  # noqa: E402

HEADER = re.compile(r"^# frostbyte-linklog v1 robot=(\S+) role=(\S+) build=(\S+)\s*$")
COLUMNS = ["seq", "t_send_s", "rtt_ms"]


def analyse(path):
    with open(path, encoding="utf-8", newline="") as f:
        head = f.readline()
        m = HEADER.match(head)
        if not m:
            raise ValueError(f"{path}: keine Kopfzeile 'frostbyte-linklog v1' (gefunden: {head.strip()!r})")
        cols = f.readline().strip().split(",")
        if cols != COLUMNS:
            raise ValueError(f"{path}: Spalten {cols}, erwartet {COLUMNS}")
        rtts, lost, run, longest, sent = [], 0, 0, 0, 0
        for n, row in enumerate(csv.reader(f), start=3):
            if not row:
                continue
            if len(row) != 3:
                raise ValueError(f"{path}:{n}: {len(row)} Zellen statt 3")
            try:
                int(row[0])
                float(row[1])
                rtt = float(row[2]) if row[2] != "" else None
            except ValueError:
                raise ValueError(f"{path}:{n}: Zahl erwartet: {row}") from None
            sent += 1
            if rtt is None:
                lost += 1
                run += 1
                longest = max(longest, run)
            else:
                rtts.append(rtt)
                run = 0
    return {
        "robot": m.group(1), "role": m.group(2), "build": m.group(3),
        "sent": sent, "received": len(rtts), "lost": lost,
        "loss_rate": lost / sent if sent else None,
        "max_consecutive_lost": longest,
        "rtt_ms": {"min": min(rtts), "median": percentile(rtts, 50), "p95": percentile(rtts, 95), "max": max(rtts)}
        if rtts else None,
    }


def _fmt(v):
    return "–" if v is None else f"{v:.1f}"


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("log")
    ap.add_argument("--max-loss", type=float, help="größte erlaubte Verlustrate (0..1)")
    ap.add_argument("--max-p95-ms", type=float, help="größte erlaubte RTT im 95-%%-Perzentil")
    ap.add_argument("--json", help="Ergebnis zusätzlich als JSON schreiben")
    a = ap.parse_args(argv)
    r = analyse(a.log)
    print(f"Roboter {r['robot']} ({r['role']}), build {r['build']}")
    print(f"gesendet {r['sent']}, beantwortet {r['received']}, verloren {r['lost']}"
          f" (Rate {_fmt(None if r['loss_rate'] is None else r['loss_rate'] * 100)} %), längste Serie {r['max_consecutive_lost']}")
    t = r["rtt_ms"] or {}
    print(f"RTT ms: min {_fmt(t.get('min'))}  Median {_fmt(t.get('median'))}  P95 {_fmt(t.get('p95'))}  max {_fmt(t.get('max'))}")
    if a.json:
        with open(a.json, "w", encoding="utf-8") as f:
            json.dump(r, f, indent=2)
    bad = []
    if a.max_loss is not None and (r["loss_rate"] is None or r["loss_rate"] > a.max_loss):
        bad.append(f"Verlustrate {_fmt(None if r['loss_rate'] is None else r['loss_rate'] * 100)} % über {a.max_loss * 100:.1f} %"
                   if r["loss_rate"] is not None else "nichts gemessen")
    if a.max_p95_ms is not None and (t.get("p95") is None or t["p95"] > a.max_p95_ms):
        bad.append(f"P95 {_fmt(t.get('p95'))} ms über {a.max_p95_ms} ms" if t else "keine Antworten")
    for b in bad:
        print("NICHT BESTANDEN:", b)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
