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


ROOT_DIR = "/work/bucu"


def lizard_row(path, name, ccn, start, end):
    return f'3,{ccn},38,2,3,"{name}@{start}-{end}@{path}","{path}","{name}","{name}()",{start},{end}\n'


def cov_function(path, start, regions):
    return {"name": "f", "filenames": [f"{ROOT_DIR}/{path}"], "regions": regions, "count": 1}


def export_of(*functions):
    return {"data": [{"functions": list(functions)}]}


class ParseTest(unittest.TestCase):
    def test_lizard_rows_become_functions(self):
        text = lizard_row("src/core/url.cpp", "bucu::core::trim", 5, 20, 28)
        self.assertEqual(
            crap.parse_lizard(text),
            [crap.Function("src/core/url.cpp", "bucu::core::trim", 20, 28, 5)],
        )

    def test_coverage_groups_by_relative_file_and_start_line_for_src_and_include_only(self):
        export = export_of(
            cov_function("src/a.cpp", 1, [region(10, 1)]),
            cov_function("src/a.cpp", 1, [region(10, 0)]),
            cov_function("third_party/x.cpp", 1, [region(1, 1)]),
            cov_function("tests/t.cpp", 1, [region(1, 1)]),
            cov_function("include/bucu/h.hpp", 1, [region(4, 1)]),
        )
        groups = crap.coverage_groups(export, ROOT_DIR)
        self.assertEqual(sorted(groups), [("include/bucu/h.hpp", 4), ("src/a.cpp", 10)])
        self.assertEqual(len(groups[("src/a.cpp", 10)]), 2)


class JoinTest(unittest.TestCase):
    def join(self, lizard, *functions):
        groups = crap.coverage_groups(export_of(*functions), ROOT_DIR)
        return crap.join(crap.parse_lizard("".join(lizard)), groups)

    def test_exact_start_line_matches(self):
        rows, no_cov, no_lizard = self.join(
            [lizard_row("src/a.cpp", "f", 3, 10, 20)],
            cov_function("src/a.cpp", 10, [region(10, 1), region(11, 0)]),
        )
        self.assertEqual((no_cov, no_lizard), ([], []))
        self.assertEqual(len(rows), 1)
        self.assertEqual((rows[0].covered, rows[0].total), (1, 2))
        self.assertAlmostEqual(rows[0].crap, crap.crap_score(3, 0.5))

    def test_start_line_inside_the_lizard_range_matches_and_merges_into_the_function(self):
        rows, no_cov, no_lizard = self.join(
            [lizard_row("src/a.cpp", "f", 2, 10, 30)],
            cov_function("src/a.cpp", 10, [region(10, 1)]),
            cov_function("src/a.cpp", 15, [region(15, 0)]),
        )
        self.assertEqual((no_cov, no_lizard), ([], []))
        self.assertEqual((rows[0].covered, rows[0].total), (1, 2))

    def test_innermost_lizard_function_wins(self):
        rows, no_cov, _ = self.join(
            [lizard_row("src/a.cpp", "outer", 2, 10, 40), lizard_row("src/a.cpp", "inner", 1, 20, 25)],
            cov_function("src/a.cpp", 22, [region(22, 1)]),
        )
        self.assertEqual([r.function.name for r in rows], ["inner"])
        self.assertEqual([f.name for f in no_cov], ["outer"])

    def test_same_line_range_in_another_file_does_not_match(self):
        rows, no_cov, no_lizard = self.join(
            [lizard_row("src/a.cpp", "f", 3, 10, 20)],
            cov_function("src/b.cpp", 10, [region(10, 1)]),
        )
        self.assertEqual(rows, [])
        self.assertEqual([f.name for f in no_cov], ["f"])
        self.assertEqual(no_lizard, [("src/b.cpp", 10)])

    def test_unmatched_function_is_listed_and_scores_as_uncovered(self):
        rows, no_cov, _ = self.join([lizard_row("src/a.cpp", "f", 4, 10, 20)])
        self.assertEqual([f.name for f in no_cov], ["f"])
        self.assertEqual(rows, [])


class RenderTest(unittest.TestCase):
    def test_table_is_sorted_by_crap_and_lists_unmatched_apart(self):
        groups = crap.coverage_groups(
            export_of(
                cov_function("src/a.cpp", 1, [region(1, 1)]),
                cov_function("src/a.cpp", 10, [region(10, 0)]),
                cov_function("src/z.cpp", 5, [region(5, 1)]),
            ),
            ROOT_DIR,
        )
        lizard = crap.parse_lizard(
            lizard_row("src/a.cpp", "easy", 2, 1, 3)
            + lizard_row("src/a.cpp", "hard", 6, 10, 20)
            + lizard_row("src/b.cpp", "gone", 1, 1, 1)
        )
        text = crap.render(*crap.join(lizard, groups))
        self.assertLess(text.index("hard"), text.index("easy"))
        self.assertIn("CRAP = CCN", text)
        self.assertLess(text.index("easy"), text.index("Not matched"))
        self.assertIn("gone", text.split("Not matched")[1])
        self.assertIn("src/z.cpp:5", text.split("Not matched")[1])


if __name__ == "__main__":
    unittest.main()
