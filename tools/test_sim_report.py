import json
import os
import tempfile
import unittest

import sim_report

COLS = ("t_s,robot,x_in,y_in,h_deg,gx_in,gy_in,gh_deg,g_err_in,tx_in,ty_in,th_deg,lift_deg,arm_deg,claw_in,"
        "lift_set_deg,arm_set_deg,cycle,route_step,has_cup,has_pin,pin_yellow_up,score,cpu_ms,batt_v,temp_max_c,event")


def row(t, robot, est=None, gps=None, truth=None, score="", event=""):
    c = [""] * 27
    c[0], c[1] = f"{t:.3f}", robot
    if est:
        c[2], c[3], c[4] = (str(v) for v in est)
    if gps:
        c[5], c[6], c[7], c[8] = (str(v) for v in gps)
    if truth:
        c[9], c[10], c[11] = (str(v) for v in truth)
    c[22], c[26] = str(score), event
    return ",".join(c)


def write_run(d, i, robot, rows):
    with open(os.path.join(d, f"run{i:03d}_{robot}.csv"), "w", encoding="utf-8") as f:
        f.write(f"# frostbyte-log v1 robot={robot} source=sim seed={i} build=demo\n{COLS}\n")
        f.write("\n".join(rows) + "\n")


class FailureTest(unittest.TestCase):
    def test_timeouts_per_state(self):
        with tempfile.TemporaryDirectory() as d:
            write_run(d, 0, "stacker", [row(0.0, "stacker", event="TIMEOUT:GREIFEN"),
                                        row(0.02, "stacker", event="TIMEOUT:GREIFEN;BLOCKED"),
                                        row(0.04, "stacker", event="TIMEOUT:HEBEN")])
            write_run(d, 1, "stacker", [row(0.0, "stacker", event="TIMEOUT:GREIFEN"), row(0.02, "stacker")])
            write_run(d, 2, "stacker", [row(0.0, "stacker")])
            m = sim_report.run_metrics(os.path.join(d, "run000_stacker.csv"))
            self.assertEqual(m["timeouts"], {"GREIFEN": 2, "HEBEN": 1})
            f = sim_report.summarize(d)["robots"]["stacker"]["failures_by_state"]
            self.assertEqual(f, {"GREIFEN": {"total": 3, "runs": 2}, "HEBEN": {"total": 1, "runs": 1}})
            self.assertIn("GREIFEN", sim_report.html_report(sim_report.summarize(d), []))


class PercentileTest(unittest.TestCase):
    def test_interpolation(self):
        self.assertEqual(sim_report.percentile([1, 2, 3, 4, 5], 50), 3)
        self.assertAlmostEqual(sim_report.percentile([0, 10], 10), 1.0)
        self.assertEqual(sim_report.percentile([7], 90), 7)

    def test_empty(self):
        self.assertIsNone(sim_report.percentile([], 50))


class MetricsTest(unittest.TestCase):
    def test_counts_errors_and_gps(self):
        with tempfile.TemporaryDirectory() as d:
            write_run(d, 0, "stacker", [
                row(0.0, "stacker", est=(0, 0, 0), gps=(0, 0, 0, 0.5), truth=(0, 0, 0)),
                row(0.02, "stacker", est=(3, 4, 0), truth=(0, 0, 0), event="COLLISION:wall"),
                row(0.04, "stacker", est=(0, 0, 0), gps=(0, 0, 0, 0.5), truth=(0, 0, 0),
                    event="COLLISION:runner;RULE:SG2"),
                row(0.06, "stacker", est=(0, 0, 0), truth=(0, 0, 0)),
            ])
            m = sim_report.run_metrics(os.path.join(d, "run000_stacker.csv"))
            self.assertEqual(m["rows"], 4)
            self.assertAlmostEqual(m["odo_err_max_in"], 5.0)
            self.assertAlmostEqual(m["odo_err_final_in"], 0.0)
            self.assertAlmostEqual(m["gps_valid_fraction"], 0.5)
            self.assertEqual(m["wall_contacts"], 1)
            self.assertEqual(m["robot_contacts"], 1)
            self.assertEqual(m["rule_violations"], 1)
            self.assertIsNone(m["score"])

    def test_score_is_last_value(self):
        with tempfile.TemporaryDirectory() as d:
            write_run(d, 0, "runner", [row(0.0, "runner", score=0), row(0.02, "runner", score=14)])
            self.assertEqual(sim_report.run_metrics(os.path.join(d, "run000_runner.csv"))["score"], 14)

    def test_robot_without_pose_has_no_error(self):
        with tempfile.TemporaryDirectory() as d:
            write_run(d, 0, "runner", [row(0.0, "runner"), row(0.02, "runner")])
            m = sim_report.run_metrics(os.path.join(d, "run000_runner.csv"))
            self.assertIsNone(m["odo_err_max_in"])
            self.assertIsNone(m["gps_valid_fraction"])


class SummaryTest(unittest.TestCase):
    def make_dir(self, d, runs, violations_in_run=None):
        for i in range(runs):
            ev = "RULE:SG6" if i == violations_in_run else ""
            write_run(d, i, "stacker", [row(0.0, "stacker", est=(0, 0, 0), truth=(0, 0, 0), score=i * 10, event=ev)])
            write_run(d, i, "runner", [row(0.0, "runner", est=(0, 0, 0), truth=(0, 0, 0), score=i * 5)])

    def test_summary_percentiles_over_runs(self):
        with tempfile.TemporaryDirectory() as d:
            self.make_dir(d, 11)
            s = sim_report.summarize(d)
            self.assertEqual(s["runs"], 11)
            st = s["robots"]["stacker"]["score"]
            self.assertEqual(st["median"], 50)
            self.assertAlmostEqual(st["p10"], 10.0)
            self.assertAlmostEqual(st["p90"], 90.0)
            self.assertEqual(s["rule_violations"], 0)

    def test_missing_score_is_none_not_zero(self):
        with tempfile.TemporaryDirectory() as d:
            write_run(d, 0, "stacker", [row(0.0, "stacker")])
            write_run(d, 0, "runner", [row(0.0, "runner")])
            self.assertIsNone(sim_report.summarize(d)["robots"]["stacker"]["score"])

    def test_no_runs_is_error(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaises(SystemExit):
                sim_report.summarize(d)


class AcceptanceTest(unittest.TestCase):
    def test_rule_violation_fails(self):
        with tempfile.TemporaryDirectory() as d:
            SummaryTest().make_dir(d, 3, violations_in_run=1)
            s = sim_report.summarize(d)
            self.assertEqual(s["rule_violations"], 1)
            ok, msgs = sim_report.acceptance(s, {})
            self.assertFalse(ok)
            self.assertTrue(any("RULE" in m or "Regel" in m for m in msgs))

    def test_target_p10_checked_when_score_exists(self):
        with tempfile.TemporaryDirectory() as d:
            SummaryTest().make_dir(d, 11)  # Stacker-P10 = 10
            s = sim_report.summarize(d)
            self.assertFalse(sim_report.acceptance(s, {"stacker": 120})[0])
            self.assertTrue(sim_report.acceptance(s, {"stacker": 10})[0])

    def test_target_not_checkable_without_score_is_reported(self):
        with tempfile.TemporaryDirectory() as d:
            write_run(d, 0, "stacker", [row(0.0, "stacker")])
            write_run(d, 0, "runner", [row(0.0, "runner")])
            ok, msgs = sim_report.acceptance(sim_report.summarize(d), {"stacker": 120})
            self.assertTrue(ok)  # kein Verstoß ...
            self.assertTrue(any("nicht geprüft" in m for m in msgs))  # ... aber ausdrücklich nicht geprüft


class RouteTargetTest(unittest.TestCase):
    def test_reads_target_p10(self):
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, "a.route")
            with open(p, "w", encoding="utf-8") as f:
                f.write("# Kommentar\ntarget_p10 120\nstart START_STACKER\n")
            self.assertEqual(sim_report.route_target(p), 120)
            self.assertIsNone(sim_report.route_target(os.path.join(d, "fehlt.route")))


class OutputTest(unittest.TestCase):
    def test_writes_json_and_html(self):
        with tempfile.TemporaryDirectory() as d:
            SummaryTest().make_dir(d, 3)
            code = sim_report.main([d, "--summary", os.path.join(d, "summary.json"), "--html", os.path.join(d, "r.html")])
            self.assertEqual(code, 0)
            with open(os.path.join(d, "summary.json"), encoding="utf-8") as f:
                self.assertEqual(json.load(f)["runs"], 3)
            with open(os.path.join(d, "r.html"), encoding="utf-8") as f:
                html = f.read()
            self.assertIn("<table", html)
            self.assertIn("stacker", html)
            self.assertNotIn("<script", html)
            self.assertNotIn("SIM-10", html)  # Hinweis auf den fehlenden Scorer ist weg (SIM-20)

    def test_html_hint_only_without_score(self):
        with tempfile.TemporaryDirectory() as d:
            write_run(d, 0, "stacker", [row(0.0, "stacker", est=(0, 0, 0), truth=(0, 0, 0))])
            sim_report.main([d, "--html", os.path.join(d, "r.html")])
            with open(os.path.join(d, "r.html"), encoding="utf-8") as f:
                self.assertIn("kein Punktestand im Log", f.read())
        with tempfile.TemporaryDirectory() as d:
            write_run(d, 0, "stacker", [row(0.0, "stacker", est=(0, 0, 0), truth=(0, 0, 0), score=25)])
            sim_report.main([d, "--html", os.path.join(d, "r.html")])
            with open(os.path.join(d, "r.html"), encoding="utf-8") as f:
                self.assertNotIn("kein Punktestand im Log", f.read())


if __name__ == "__main__":
    unittest.main()
