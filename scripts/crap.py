#!/usr/bin/env python3
"""CRAP score of each function: complexity from lizard joined to coverage from llvm-cov.

CRAP = CCN^2 * (1 - c)^3 + CCN, with c the region coverage of the function (ADR 0011).
"""

import argparse
import csv
import io
import json
import sys
from dataclasses import dataclass

# Only these source trees are reported; third_party/ and tests/ are not.
REPORTED_DIRS = ("src/", "include/")

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


@dataclass(frozen=True)
class Function:
    """A function as lizard reports it."""

    file: str
    name: str
    start: int
    end: int
    ccn: int


@dataclass(frozen=True)
class Row:
    """A lizard function joined to its merged coverage."""

    function: Function
    covered: int
    total: int

    @property
    def coverage(self):
        return fraction(self.covered, self.total)

    @property
    def crap(self):
        return crap_score(self.function.ccn, self.coverage)


def parse_lizard(csv_text):
    """Functions from `lizard --csv`: NLOC, CCN, tokens, params, length, location, file, name, ..."""
    functions = []
    for row in csv.reader(io.StringIO(csv_text)):
        functions.append(Function(row[6], row[7], int(row[9]), int(row[10]), int(row[1])))
    return functions


def coverage_groups(export, root):
    """Group the region lists of `llvm-cov export` JSON by (relative file, start line).

    Instantiations of one source function share a group. Only src/ and include/ are kept.
    """
    prefix = root.rstrip("/") + "/"
    groups = {}
    for data in export["data"]:
        for function in data["functions"]:
            path = function["filenames"][0]
            if not path.startswith(prefix) or not function["regions"]:
                continue
            relative = path[len(prefix):]
            if not relative.startswith(REPORTED_DIRS):
                continue
            key = (relative, function["regions"][0][LINE_START])
            groups.setdefault(key, []).append(function["regions"])
    return groups


def _owner(functions, file, line):
    """The innermost function of `file` whose range holds `line`; an exact start wins."""
    inside = [f for f in functions if f.file == file and f.start <= line <= f.end]
    exact = [f for f in inside if f.start == line]
    if exact:
        return exact[0]
    return min(inside, key=lambda f: f.end - f.start, default=None)


def join(functions, groups):
    """Join lizard functions to coverage groups by file and line range.

    Returns (rows, functions without coverage, coverage groups without a function). A
    function that owns several groups (lambdas) gets their regions together.
    """
    owned = {}
    orphans = []
    for (file, line), instantiations in sorted(groups.items()):
        owner = _owner(functions, file, line)
        if owner is None:
            orphans.append((file, line))
        else:
            owned.setdefault(owner, []).extend(instantiations)
    rows = []
    for function, instantiations in owned.items():
        covered, total = region_coverage(instantiations)
        rows.append(Row(function, covered, total))
    rows.sort(key=lambda r: (-r.crap, r.function.file, r.function.start))
    without = [f for f in functions if f not in owned]
    return rows, without, orphans


def render(rows, without_coverage, without_function):
    """Markdown report: functions by CRAP score, then what could not be joined."""
    out = [
        "# CRAP score report",
        "",
        "CRAP = CCN^2 * (1 - c)^3 + CCN, with CCN the cyclomatic complexity from lizard and c "
        "the region coverage of the function from llvm-cov. A region counts as covered when "
        "any instantiation covers it (ADR 0011).",
        "",
        "| CRAP | CCN | Coverage | Function | Location |",
        "| ---: | ---: | ---: | --- | --- |",
    ]
    for r in rows:
        f = r.function
        out.append(
            f"| {r.crap:.1f} | {f.ccn} | {r.coverage:.0%} | `{f.name}` | {f.file}:{f.start} |"
        )
    out += ["", "## Not matched", "", "Functions lizard found that have no coverage data:", ""]
    out += [f"- `{f.name}` ({f.file}:{f.start})" for f in without_coverage] or ["- none"]
    out += ["", "Coverage entries with no lizard function (implicit or defaulted members):", ""]
    out += [f"- {file}:{line}" for file, line in without_function] or ["- none"]
    return "\n".join(out) + "\n"


def main(argv):
    parser = argparse.ArgumentParser(description="Join lizard CSV to llvm-cov JSON into crap.md.")
    parser.add_argument("--lizard", required=True, help="lizard --csv output")
    parser.add_argument("--coverage", required=True, help="llvm-cov export JSON")
    parser.add_argument("--root", required=True, help="repository root the JSON paths start with")
    parser.add_argument("--out", required=True, help="Markdown report to write")
    args = parser.parse_args(argv)
    with open(args.lizard, encoding="utf-8") as f:
        functions = parse_lizard(f.read())
    with open(args.coverage, encoding="utf-8") as f:
        groups = coverage_groups(json.load(f), args.root)
    with open(args.out, "w", encoding="utf-8") as f:
        f.write(render(*join(functions, groups)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
