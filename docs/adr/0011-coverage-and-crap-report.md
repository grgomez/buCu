# ADR 0011: Source-based coverage and a CRAP report

- Status: Accepted
- Date: 2026-11-19
- Issue: #94

## Context

The code base needs a view of which code its tests reach and which functions are both complex and poorly tested, so cruft shows up before a review finds it. The view must run on the toolchain CI already uses (LLVM 19 for clang-tidy and the fuzz targets), fit the CMake presets, and stay offline. Coverage alone does not say where the risk is, so the report also needs a complexity number for each function. Trials on this repository (issue #94) compared the options.

## Considered options

- **llvm-cov source-based coverage (chosen):** `-fprofile-instr-generate -fcoverage-mapping`, `llvm-profdata`, and `llvm-cov` from LLVM 19. Exact regions and branches, a per-function JSON export, and a run that takes seconds. It does not work with MSVC, and Apple Clang needs `xcrun llvm-cov`.
- **gcov with gcovr or lcov:** works with GCC on every Unix, but it is a second toolchain beside LLVM 19. gcovr on LLVM data took about 5 minutes, GCC branch counts include exception edges (63% against 85.8%), and gcovr computes no complexity.
- **lizard for complexity (chosen):** reads the sources without a build, parses the C++23 code, and reports cyclomatic complexity (CCN) with function names and line ranges. It is a Python tool with an MIT licence, not vendored.
- **clang-tidy `readability-function-cognitive-complexity`:** already available, but excluded in `.clang-tidy` because it scores Catch2 macro expansion, it is a different metric, and it reports only above a threshold, so there is no value to join with coverage.
- **A gate on thresholds now:** no baseline exists, and a first threshold would be a guess.

## Decision

We will build a `coverage` preset (configure, build, and test) with clang source-based coverage on Linux and macOS, and write the report with `scripts/coverage.sh [build-dir]`. The script merges the profiles that `ctest --preset coverage` leaves in `build/coverage/profiles/`, writes the `llvm-cov` report, lcov and JSON exports, and an HTML report for `src/` and `include/` (never `third_party/` or `tests/`), runs lizard 1.26.1, and calls `scripts/crap.py`. `LLVM_COV`, `LLVM_PROFDATA`, and `LIZARD` override the tool paths, as `CLANG_TIDY` does for `scripts/tidy.sh`.

`scripts/crap.py` computes CRAP = CCN^2 * (1 - c)^3 + CCN for each function, with c the region coverage of the function. A region counts as covered when any instantiation of the function covers it. It joins lizard and llvm-cov on the same file with the llvm-cov start line inside the lizard line range, merges a lambda into the function that holds it, and lists the functions that do not join apart, in `crap.md`, sorted by score.

A `coverage` job in `ci.yml` runs this on Ubuntu with clang-19, writes the totals and the highest CRAP scores to the job summary, and uploads the report. The report is informational: the job fails only when a tool fails, never on a threshold. Thresholds are a later decision, once a baseline exists.

## Consequences

- Every PR shows the coverage totals and the 20 functions with the highest CRAP score.
- The preset is not available on Windows. Windows-only code (`utf16.hpp`, parts of `console.cpp`) never shows as covered, and the report lists it as not matched.
- lizard is a new development tool (architecture section 2). The script names it and exits 2 when it is missing, and CI installs the pinned version.
- Coverage can fall without failing CI until a threshold exists; the job summary makes a fall visible on each PR.
- Error-propagation branches that tests cannot trigger lower the numbers without pointing at untested behaviour; the audit in #94 records this for a later decision.
