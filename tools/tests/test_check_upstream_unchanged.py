import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).parents[1] / "check_upstream_unchanged.py"


class UpstreamCheckTests(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.repo = Path(self.temporary_directory.name)
        self.run_git("init", "--quiet")
        self.run_git("config", "user.name", "Test User")
        self.run_git("config", "user.email", "test@example.invalid")
        (self.repo / "Upstream").mkdir()
        (self.repo / "Upstream" / "core.cpp").write_text("original\n", encoding="utf-8")
        self.run_git("add", ".")
        self.run_git("commit", "--quiet", "-m", "snapshot")

    def tearDown(self):
        self.temporary_directory.cleanup()

    def run_git(self, *arguments):
        subprocess.run(["git", *arguments], cwd=self.repo, check=True)

    def check(self, reference="HEAD", cwd=None):
        return subprocess.run(
            [sys.executable, str(SCRIPT), reference],
            cwd=cwd or self.repo,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )

    def test_succeeds_for_unchanged_tree_from_nested_directory(self):
        nested = self.repo / "nested"
        nested.mkdir()
        result = self.check(cwd=nested)
        self.assertEqual(0, result.returncode, result.stderr)

    def test_reports_modified_and_untracked_paths(self):
        (self.repo / "Upstream" / "core.cpp").write_text("modified\n", encoding="utf-8")
        (self.repo / "Upstream" / "new.cpp").write_text("new\n", encoding="utf-8")
        result = self.check()
        self.assertEqual(1, result.returncode)
        self.assertIn("Upstream/core.cpp", result.stderr)
        self.assertIn("Upstream/new.cpp", result.stderr)

    def test_reports_invalid_reference(self):
        result = self.check("not-a-reference")
        self.assertEqual(2, result.returncode)
        self.assertIn("unavailable or invalid", result.stderr)


if __name__ == "__main__":
    unittest.main()
