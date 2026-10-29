"""Check evidence and failure handling independently of the GUI runtime."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("gui_runner", ROOT / "tests/run.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class EvidenceTests(unittest.TestCase):
    def run_fake(self, temporary, failure=""):
        script = Path(temporary) / "fake_lisp.py"
        script.write_text(
            "import os, pathlib, sys\n"
            "name = pathlib.Path(sys.argv[-1]).name\n"
            "print('fake runtime output for ' + name, flush=True)\n"
            "sys.exit(7 if name == os.environ.get('WXLAMBDA_FAKE_FAIL') else 0)\n",
            encoding="utf-8")
        return subprocess.run(
            [sys.executable, str(ROOT / "tests/run.py"), sys.executable, str(script)],
            env={**os.environ, "WXLAMBDA_TEST_REPORT": str(Path(temporary) / "report"),
                 "WXLAMBDA_FAKE_FAIL": failure}, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, timeout=15)

    def test_success_records_all_scenarios_and_preflight_separately(self):
        with tempfile.TemporaryDirectory() as temporary:
            result = self.run_fake(temporary)
            self.assertEqual(result.returncode, 0, result.stdout)
            report = Path(temporary) / "report"
            summary = json.loads((report / "summary.json").read_text())
            self.assertEqual(summary["status"], "passed")
            self.assertEqual(summary["completed_gui_scenarios"], 71)
            self.assertEqual(summary["expected_gui_scenarios"], 71)
            self.assertEqual(len(summary["results"]), 72)
            self.assertEqual(summary["results"][0]["kind"], "preflight")
            for row in summary["results"]:
                self.assertIn(b"fake runtime output", (report / row["log"]).read_bytes())

    def test_failure_preserves_status_output_and_stops(self):
        with tempfile.TemporaryDirectory() as temporary:
            result = self.run_fake(temporary, failure="api.lisp")
            self.assertEqual(result.returncode, 7, result.stdout)
            report = Path(temporary) / "report"
            summary = json.loads((report / "summary.json").read_text())
            self.assertEqual(summary["status"], "failed")
            self.assertEqual(summary["completed_gui_scenarios"], 3)
            self.assertEqual(summary["results"][-1]["name"], "api")
            self.assertEqual(summary["results"][-1]["exit_code"], 7)
            self.assertIn(b"api.lisp", (report / "api.log").read_bytes())
            self.assertFalse((report / "layout.log").exists())

    def test_preflight_failure_never_claims_gui_coverage(self):
        with tempfile.TemporaryDirectory() as temporary:
            result = self.run_fake(temporary, failure="platform.lisp")
            self.assertEqual(result.returncode, 7, result.stdout)
            report = Path(temporary) / "report"
            summary = json.loads((report / "summary.json").read_text())
            self.assertEqual(summary["status"], "failed")
            self.assertEqual(summary["completed_gui_scenarios"], 0)
            self.assertEqual(len(summary["results"]), 1)
            self.assertEqual(summary["results"][0]["kind"], "preflight")
            self.assertFalse((report / "smoke-normal.log").exists())

    def test_stale_report_directory_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            self.assertEqual(self.run_fake(temporary).returncode, 0)
            summary = Path(temporary) / "report/summary.json"
            original = summary.read_bytes()
            self.assertNotEqual(self.run_fake(temporary).returncode, 0)
            self.assertEqual(summary.read_bytes(), original)

    def test_timeout_preserves_partial_output(self):
        with patch.object(runner.subprocess, "run", side_effect=subprocess.TimeoutExpired(
                ["fake"], 30, output=b"partial output")):
            code, output, status = runner.run_case(["fake"], Path("test.lisp"), {})
        self.assertEqual((code, status), (124, "timeout"))
        self.assertIn(b"partial output", output)
        self.assertIn(b"timed out", output)

    def test_missing_lisp_is_a_failure(self):
        code, output, status = runner.run_case(
            [str(ROOT / "nonexistent-wxlambda-test-lisp")], Path("test.lisp"), {})
        self.assertEqual((code, status), (127, "failed"))
        self.assertIn(b"Cannot launch Lisp", output)


if __name__ == "__main__":
    unittest.main()
