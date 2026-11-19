#!/usr/bin/env python3
"""CRAP score of each function: complexity from lizard joined to coverage from llvm-cov.

CRAP = CCN^2 * (1 - c)^3 + CCN, with c the region coverage of the function (ADR 0011).
"""

# llvm-cov region kind of an ordinary code region; other kinds are expansions, skipped
# lines, gaps, and branch regions.
CODE_REGION = 0
# Indexes into an llvm-cov region: [line_start, col_start, line_end, col_end, count, ...].
LINE_START, COL_START, LINE_END, COL_END, COUNT, KIND = 0, 1, 2, 3, 4, 7


def crap_score(ccn, coverage):
    """CRAP score of a function with cyclomatic complexity `ccn` and coverage in [0, 1]."""
    return ccn**2 * (1 - coverage) ** 3 + ccn


def fraction(covered, total):
    """Covered share of `total`; a function with no regions counts as fully covered."""
    return 1.0 if total == 0 else covered / total


def region_coverage(instantiations):
    """Return (covered, total) code regions over the regions of every instantiation.

    Each item of `instantiations` is the region list of one instantiation of the same
    source function. A region counts as covered when any instantiation covers it.
    """
    covered_by = {}
    for regions in instantiations:
        for r in regions:
            if r[KIND] != CODE_REGION:
                continue
            key = (r[LINE_START], r[COL_START], r[LINE_END], r[COL_END])
            covered_by[key] = covered_by.get(key, False) or r[COUNT] > 0
    return sum(covered_by.values()), len(covered_by)
