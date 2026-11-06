import os
import tempfile
import unittest

import drive_check

COLS = ("t_s,robot,x_in,y_in,h_deg,gx_in,gy_in,gh_deg,g_err_in,tx_in,ty_in,th_deg,lift_deg,arm_deg,claw_in,"
        "lift_set_deg,arm_set_deg,cycle,route_step,has_cup,has_pin,pin_yellow_up,score,cpu_ms,batt_v,temp_max_c,event")


def row(t, est=None, truth=None):
    c = [""] * 27
    c[0], c[1] = f"{t:.3f}", "stacker"
    if est:
        c[2], c[3], c[4] = (str(v) for v in est)
    if truth:
        c[9], c[10], c[11] = (str(v) for v in truth)
    return ",".join(c)


def log(rows, source="sim"):
    return f"# frostbyte-log v1 robot=stacker source={source} seed=1 build=x\n{COLS}\n" + "\n".join(rows) + "\n"


STRAIGHT = "start pose x=-39.37 y=0 h=0\ngoto pose x=39.37 y=0 h=0 timeout=8\nend hold\n"
TURNS = "start pose x=-39.37 y=0 h=0\nrepeat 10 {\n turn 90 timeout=3\n turn 0 timeout=3\n}\nend hold\n"


def files(text, route):
    d = tempfile.mkdtemp()
    lp, rp = os.path.join(d, "l.csv"), os.path.join(d, "r.route")
    open(lp, "w", encoding="utf-8").write(text)
    open(rp, "w", encoding="utf-8").write(route)
    return lp, rp


class StraightTest(unittest.TestCase):
    def test_pass_with_truth(self):
        # 1 cm Längsfehler, 1 cm quer: Abstand ca. 1,4 cm
        lp, rp = files(log([row(0, truth=(-39.37, 0, 0)), row(5, truth=(39.37 - 0.394, 0.394, 0))]), STRAIGHT)
        r = drive_check.check("straight", lp, rp)
        self.assertTrue(r.passed)
        self.assertAlmostEqual(r.value, 1.41, delta=0.05)
        self.assertEqual(r.unit, "cm")

    def test_fail_with_truth(self):
        lp, rp = files(log([row(5, truth=(39.37 - 1.5, 0.0, 0))]), STRAIGHT)  # 3,8 cm zu kurz
        r = drive_check.check("straight", lp, rp)
        self.assertFalse(r.passed)
        self.assertAlmostEqual(r.value, 3.81, delta=0.05)

    def test_limit_is_inclusive(self):
        lp, rp = files(log([row(5, truth=(39.37 - 2.0 / 2.54, 0.0, 0))]), STRAIGHT)
        self.assertTrue(drive_check.check("straight", lp, rp).passed)

    def test_robot_log_needs_measurement(self):
        lp, rp = files(log([row(5, est=(39.37, 0, 0))], source="robot"), STRAIGHT)
        with self.assertRaises(drive_check.CheckError):
            drive_check.check("straight", lp, rp)  # Odometrie allein ist kein Nachweis

    def test_robot_log_with_measurement(self):
        lp, rp = files(log([row(5, est=(39.37, 0, 0))], source="robot"), STRAIGHT)
        self.assertTrue(drive_check.check("straight", lp, rp, measured=1.5).passed)
        r = drive_check.check("straight", lp, rp, measured=2.5)
        self.assertFalse(r.passed)
        self.assertEqual(r.value, 2.5)

    def test_measurement_overrides_truth(self):
        lp, rp = files(log([row(5, truth=(39.37, 0, 0))]), STRAIGHT)
        self.assertFalse(drive_check.check("straight", lp, rp, measured=3.0).passed)

    def test_odometry_reported_separately(self):
        lp, rp = files(log([row(5, est=(39.0, 0, 0), truth=(39.37, 0, 0))]), STRAIGHT)
        r = drive_check.check("straight", lp, rp)
        self.assertAlmostEqual(r.odometry, 0.37 * 2.54, delta=0.01)


class TurnsTest(unittest.TestCase):
    def test_pass_and_wrap(self):
        lp, rp = files(log([row(5, truth=(-39.37, 0, 359.0))]), TURNS)  # 1 Grad vor 0
        r = drive_check.check("turns", lp, rp)
        self.assertTrue(r.passed)
        self.assertAlmostEqual(r.value, 1.0, delta=0.001)
        self.assertEqual(r.unit, "deg")

    def test_fail(self):
        lp, rp = files(log([row(5, truth=(-39.37, 0, 2.5))]), TURNS)
        self.assertFalse(drive_check.check("turns", lp, rp).passed)

    def test_full_rotations_do_not_hide_error(self):
        lp, rp = files(log([row(5, truth=(-39.37, 0, -3597.0))]), TURNS)  # 10 Umdrehungen, 3 Grad daneben
        r = drive_check.check("turns", lp, rp)
        self.assertFalse(r.passed)
        self.assertAlmostEqual(r.value, 3.0, delta=0.001)


class InputTest(unittest.TestCase):
    def test_target_comes_from_route(self):
        lp, rp = files(log([row(5, truth=(10.0, 0, 0))]), "start pose x=0 y=0 h=0\ngoto pose x=10 y=0 h=0\nend hold\n")
        self.assertTrue(drive_check.check("straight", lp, rp).passed)

    def test_empty_log_is_error(self):
        lp, rp = files(log([]), STRAIGHT)
        with self.assertRaises(drive_check.CheckError):
            drive_check.check("straight", lp, rp)

    def test_route_without_goto_is_error(self):
        lp, rp = files(log([row(5, truth=(0, 0, 0))]), "start pose x=0 y=0 h=0\nend hold\n")
        with self.assertRaises(drive_check.CheckError):
            drive_check.check("straight", lp, rp)

    def test_main_exit_codes(self):
        lp, rp = files(log([row(5, truth=(39.37, 0, 0))]), STRAIGHT)
        self.assertEqual(drive_check.main(["straight", lp, "--route", rp]), 0)
        lp, rp = files(log([row(5, truth=(30.0, 0, 0))]), STRAIGHT)
        self.assertEqual(drive_check.main(["straight", lp, "--route", rp]), 1)
        lp, rp = files(log([row(5, est=(39.37, 0, 0))], source="robot"), STRAIGHT)
        self.assertEqual(drive_check.main(["straight", lp, "--route", rp]), 2)


if __name__ == "__main__":
    unittest.main()
