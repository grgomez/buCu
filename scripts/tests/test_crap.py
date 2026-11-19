"""Tests for scripts/crap.py."""

import importlib.util
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("crap", ROOT / "scripts" / "crap.py")
crap = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(crap)


def region(line, count, kind=0):
    """An llvm-cov region: start line and column, end line and column, count, ids, kind."""
    return [line, 1, line, 20, count, 0, 0, kind]


class CrapScoreTest(unittest.TestCase):
    def test_fully_covered_function_scores_its_complexity(self):
        self.assertEqual(crap.crap_score(5, 1.0), 5)

    def test_uncovered_function_scores_complexity_squared_plus_complexity(self):
        self.assertEqual(crap.crap_score(5, 0.0), 30)

    def test_half_covered_function(self):
        self.assertAlmostEqual(crap.crap_score(4, 0.5), 16 * 0.125 + 4)


class RegionCoverageTest(unittest.TestCase):
    def test_counts_code_regions_only(self):
        regions = [region(1, 3), region(2, 0), region(3, 0, kind=1), region(4, 0, kind=2)]
        self.assertEqual(crap.region_coverage([regions]), (1, 2))

    def test_instantiations_merge_a_region_is_covered_when_any_covers_it(self):
        first = [region(1, 4), region(2, 0)]
        second = [region(1, 0), region(2, 7)]
        self.assertEqual(crap.region_coverage([first, second]), (2, 2))

    def test_no_regions_is_fully_covered(self):
        self.assertEqual(crap.region_coverage([[]]), (0, 0))
        self.assertEqual(crap.fraction(0, 0), 1.0)


if __name__ == "__main__":
    unittest.main()
