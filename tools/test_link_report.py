import os
import tempfile
import unittest

import link_report

HEAD = "# frostbyte-linklog v1 robot=stacker role=ping build=abc\nseq,t_send_s,rtt_ms\n"


def write(d, body, head=HEAD, name="link.csv"):
    p = os.path.join(d, name)
    with open(p, "w", encoding="utf-8") as f:
        f.write(head + body)
    return p


class ReportTest(unittest.TestCase):
    def test_example_log_from_cpp_tests(self):
        # dieselbe Datei prüft test/test_link_test.cpp gegen den C++-Schreiber
        path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "test", "fixtures", "linklog_example.csv")
        r = link_report.analyse(path)
        self.assertEqual((r["sent"], r["received"], r["lost"]), (5, 4, 1))
        self.assertEqual(r["rtt_ms"]["min"], 8)
        self.assertEqual(r["rtt_ms"]["max"], 40)
        self.assertEqual(r["build"], "abc123")

    def test_latency_and_loss(self):
        rows = [f"{i},{i * 0.05:.3f},{rtt}" for i, rtt in enumerate([10, 12, 14, 16, 18, 20, 22, 24, 26, 100])]
        rows += ["10,0.500,", "11,0.550,", "12,0.600,8"]  # zwei verloren hintereinander
        with tempfile.TemporaryDirectory() as d:
            r = link_report.analyse(write(d, "\n".join(rows) + "\n"))
        self.assertEqual((r["sent"], r["received"], r["lost"]), (13, 11, 2))
        self.assertAlmostEqual(r["loss_rate"], 2 / 13)
        self.assertEqual(r["max_consecutive_lost"], 2)
        self.assertEqual(r["rtt_ms"]["min"], 8)
        self.assertEqual(r["rtt_ms"]["max"], 100)
        self.assertEqual(r["rtt_ms"]["median"], 18)
        self.assertEqual(r["robot"], "stacker")
        self.assertEqual(r["role"], "ping")

    def test_p95_value(self):
        # 21 Werte 1..21: P95 liegt bei Index 19 -> 20
        rows = [f"{i},{i * 0.05:.3f},{i + 1}" for i in range(21)]
        with tempfile.TemporaryDirectory() as d:
            r = link_report.analyse(write(d, "\n".join(rows) + "\n"))
        self.assertEqual(r["rtt_ms"]["p95"], 20)

    def test_all_lost_has_no_rtt(self):
        with tempfile.TemporaryDirectory() as d:
            r = link_report.analyse(write(d, "0,0.000,\n1,0.050,\n"))
        self.assertEqual(r["lost"], 2)
        self.assertIsNone(r["rtt_ms"])
        self.assertEqual(r["loss_rate"], 1.0)

    def test_empty_log(self):
        with tempfile.TemporaryDirectory() as d:
            r = link_report.analyse(write(d, ""))
        self.assertEqual(r["sent"], 0)
        self.assertIsNone(r["loss_rate"])
        self.assertIsNone(r["rtt_ms"])

    def test_wrong_header_is_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaises(ValueError):
                link_report.analyse(write(d, "", head="# frostbyte-log v1 robot=stacker\nt_s\n"))
            with self.assertRaises(ValueError):
                link_report.analyse(write(d, "", head="# frostbyte-linklog v2 robot=a role=ping build=x\nseq,t_send_s,rtt_ms\n"))
            with self.assertRaises(ValueError):
                link_report.analyse(write(d, "", head="# frostbyte-linklog v1 robot=a role=ping build=x\nseq,rtt_ms\n"))

    def test_broken_row_is_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaises(ValueError):
                link_report.analyse(write(d, "0,0.000,abc\n"))
            with self.assertRaises(ValueError):
                link_report.analyse(write(d, "0,0.000\n"))

    def test_limits_decide_exit_code(self):
        rows = "".join(f"{i},{i * 0.05:.3f},10\n" for i in range(9)) + "9,0.450,\n"
        with tempfile.TemporaryDirectory() as d:
            p = write(d, rows)
            self.assertEqual(link_report.main([p]), 0)  # ohne Grenzwerte nur Bericht
            self.assertEqual(link_report.main([p, "--max-loss", "0.2"]), 0)
            self.assertEqual(link_report.main([p, "--max-loss", "0.05"]), 1)
            self.assertEqual(link_report.main([p, "--max-p95-ms", "5"]), 1)
            self.assertEqual(link_report.main([p, "--max-p95-ms", "15"]), 0)

    def test_no_data_fails_limits(self):
        with tempfile.TemporaryDirectory() as d:
            p = write(d, "")
            self.assertEqual(link_report.main([p, "--max-loss", "0.5"]), 1)  # nichts gemessen ist nicht bestanden


if __name__ == "__main__":
    unittest.main()
