"""Exercise the experimental patch against a temporary production-source copy."""

import pathlib
import shutil
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class ExperimentalImeBridgeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="wecom-ime-bridge-test-")
        cls.addClassCleanup(cls.temp.cleanup)
        directory = pathlib.Path(cls.temp.name)
        source = directory / "scripts" / "ime-bridge.c"
        source.parent.mkdir()
        shutil.copyfile(ROOT / "scripts" / "ime-bridge.c", source)
        subprocess.run(
            ["patch", "--batch", "--forward", "-p1", "-d", str(directory),
             "-i", str(ROOT / "experiments" / "wine-ime-commit" / "bridge.patch")],
            check=True, capture_output=True, text=True, timeout=30,
        )
        cls.binary = directory / "bridge-probe"
        subprocess.run(
            ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
             f'-DIME_BRIDGE_SOURCE="{source}"',
             str(ROOT / "experiments" / "wine-ime-commit" / "bridge-probe.c"),
             "-o", str(cls.binary), "-ldl"],
            check=True, capture_output=True, text=True, timeout=30,
        )

    def run_case(self, case):
        result = subprocess.run(
            [str(self.binary), case], capture_output=True, text=True, timeout=10,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(f"PASS: {case}", result.stdout)

    def test_complete_commit_is_one_call_and_suppressed_after_acceptance(self):
        self.run_case("accepted")

    def test_rejected_commit_preserves_original_bytes_status_and_keysym(self):
        self.run_case("rejected")

    def test_mismatched_xim_client_does_not_send(self):
        self.run_case("client-mismatch")

    def test_changed_focus_does_not_send(self):
        self.run_case("focus-changed")

    def test_other_application_does_not_send(self):
        self.run_case("other-process")

    def test_other_window_class_does_not_send(self):
        self.run_case("class-mismatch")

    def test_same_process_window_does_not_send(self):
        self.run_case("same-process")

    def test_ordinary_key_does_not_send(self):
        self.run_case("ordinary-key")


if __name__ == "__main__":
    unittest.main()
