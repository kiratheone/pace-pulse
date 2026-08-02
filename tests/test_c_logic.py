import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class CLogicTests(unittest.TestCase):
    def compile_and_run(self, test_source, production_sources):
        with tempfile.TemporaryDirectory() as temporary_directory:
            executable = Path(temporary_directory) / "c_logic_test"
            compile_result = subprocess.run(
                [
                    "cc",
                    "-std=c99",
                    "-Wall",
                    "-Werror",
                    "-Isrc/c",
                    test_source,
                    *production_sources,
                    "-lm",
                    "-o",
                    str(executable),
                ],
                cwd=ROOT,
                capture_output=True,
                text=True,
            )
            self.assertEqual(compile_result.returncode, 0, compile_result.stderr)

            run_result = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(run_result.returncode, 0, run_result.stderr)

    def test_heart_rate_logic_executes_as_host_c_code(self):
        self.compile_and_run(
            "tests/c/test_heart_rate.c", ["src/c/heart_rate.c"]
        )

    def test_tracker_logic_executes_as_host_c_code(self):
        self.compile_and_run("tests/c/test_tracker.c", ["src/c/tracker.c"])

    def test_current_pace_uses_32_bit_window_accumulators(self):
        tracker_source = (ROOT / "src/c/tracker.c").read_text()
        self.assertIn("uint32_t total_time_ms = 0;", tracker_source)
        self.assertIn("uint32_t total_distance_mm = 0;", tracker_source)

    def test_distance_calculation_avoids_builtin_square_root(self):
        tracker_source = (ROOT / "src/c/tracker.c").read_text()
        self.assertIn("prv_fast_sqrtf", tracker_source)
        self.assertNotRegex(tracker_source, r"(?<!fast_)sqrtf\(")


if __name__ == "__main__":
    unittest.main()
