#!/usr/bin/env python3
"""Erzeugt test/fixtures/livesim_reference.csv: wahre Posen (tx_in, ty_in, th_deg) und Ereignisse der ersten 10 s,
wie frostbyte_sim sie für test/fixtures/livesim_input.csv schreibt (SIM-17, ADR-025 Nachtrag).

Aufruf (nach dem Bau von build-test):  python3 tools/make_livesim_reference.py
Die Referenz wird von test/test_sim_livesim_reference.cpp (C++ muss sie weiter erzeugen) und von
tools/viewer/test_livesim.py (der Browser-Livesim muss sie treffen) geprüft.
"""
import csv
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FIX = ROOT / "test" / "fixtures"
DURATION_S = 10.0


def main():
    sim = ROOT / "build-test" / "frostbyte_sim"
    if not sim.exists():
        sys.exit("build-test/frostbyte_sim fehlt: cmake -S test -B build-test && cmake --build build-test")
    out = ROOT / "test" / "fixtures" / "livesim_reference.csv"
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run([str(sim), "--input", str(FIX / "livesim_input.csv"), "--params", str(FIX / "sim_params_min.json"),
                        "--field", str(FIX / "field_two.json"), "--noise", "none", "--out", tmp], check=True,
                       stdout=subprocess.DEVNULL)
        with open(out, "w", newline="", encoding="utf-8") as f:
            f.write("t_s,robot,tx_in,ty_in,th_deg,event\n")
            for robot in ("stacker", "runner"):
                lines = [l for l in Path(tmp, f"run000_{robot}.csv").read_text(encoding="utf-8").splitlines()
                         if not l.startswith("#")]
                for row in csv.DictReader(lines):
                    if float(row["t_s"]) <= DURATION_S + 1e-9:
                        f.write(f'{row["t_s"]},{robot},{row["tx_in"]},{row["ty_in"]},{row["th_deg"]},{row["event"]}\n')
    print("geschrieben:", out)


if __name__ == "__main__":
    main()
