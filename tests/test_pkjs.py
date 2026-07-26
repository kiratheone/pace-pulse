import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class PebbleKitJsTests(unittest.TestCase):
    def test_position_queue_executes_in_node(self):
        result = subprocess.run(
            ["node", "tests/js/test_pkjs.js"],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
