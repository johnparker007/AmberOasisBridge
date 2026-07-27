import importlib.util
from pathlib import Path
import tempfile
import unittest

SCRIPT = Path(__file__).parents[1] / "tools" / "compare_maintained_baseline.py"
spec = importlib.util.spec_from_file_location("baseline_compare", SCRIPT)
baseline_compare = importlib.util.module_from_spec(spec)
assert spec.loader
spec.loader.exec_module(baseline_compare)


class CompareBaselineTests(unittest.TestCase):
    def test_reports_identical_missing_and_different_sources(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for upstream, maintained in baseline_compare.CORE_PATHS.values():
                (root / upstream).mkdir(parents=True)
                (root / maintained).mkdir(parents=True)
            upstream, maintained = baseline_compare.CORE_PATHS["Epoch"]
            (root / upstream / "same.cpp").write_text("same")
            (root / maintained / "same.cpp").write_text("same")
            (root / upstream / "missing.h").write_text("missing")
            (root / upstream / "changed.inc").write_text("old")
            (root / maintained / "changed.inc").write_text("new")
            (root / upstream / "ignored.vcxproj").write_text("metadata")

            identical, missing, different = baseline_compare.compare(root)

            self.assertEqual(identical, ["Epoch/same.cpp"])
            self.assertEqual(missing, ["Epoch/missing.h"])
            self.assertEqual(different, ["Epoch/changed.inc"])


if __name__ == "__main__":
    unittest.main()
