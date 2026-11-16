# Architecture decision records

Read this index in every session. Read an ADR only when the issue touches its decision. Start a new ADR from [`0000-template.md`](0000-template.md), and add its row here in the same commit. When an ADR supersedes or amends another, update both rows.

| ADR | Decision | Status |
| --- | --- | --- |
| [0001](0001-cpp20-cmake-sqlite.md) | C++20, CMake, and SQLite for a native, portable port of Buku | Accepted, amended by [0006](0006-coding-standard-google-and-core-guidelines.md) |
| [0002](0002-module-layout.md) | Six library modules: core, db, io, net, platform, cli | Accepted |
| [0003](0003-plan-mode-sdd-workflow.md) | SDD with Claude Code plan mode as the approval gate; specs live in the issue body | Accepted, amended by [0008](0008-evidence-kinds.md) |
| [0004](0004-vendored-dependencies.md) | Vendor SQLite, CLI11, and Catch2 in `third_party/` | Accepted |
| [0005](0005-libcurl-networking.md) | libcurl for the optional title and description fetch | Accepted |
| [0006](0006-coding-standard-google-and-core-guidelines.md) | Google C++ Style Guide and C++ Core Guidelines as the coding standard, with recorded exceptions and a clang-tidy gate; amends ADR 0001 (C++23) | Accepted, amended by [0007](0007-readability-modernize-misc-checks.md) |
| [0007](0007-readability-modernize-misc-checks.md) | Enable the readability, modernize, and misc clang-tidy families with six recorded exclusions; allow a whole-file `NOLINT` pair in generated tables; amends ADR 0006 | Accepted |
| [0008](0008-evidence-kinds.md) | Declare an evidence kind (test, property, fuzz, proof, check, manual, after merge) on each acceptance criterion; a property test is a test, a fuzz target or proof harness sits beside one; amends ADR 0003 | Accepted |
| [0009](0009-rapidcheck-property-tests.md) | RapidCheck, vendored at a pinned commit, for property tests of the pure `core` functions, run by ctest with a fixed seed | Accepted |
| [0010](0010-libfuzzer-fuzz-targets.md) | libFuzzer, from the LLVM Clang runtime, for fuzz targets of the `core` text parsers: a `fuzz` preset, seed corpus replayed by ctest, 60 s per PR and 600 s weekly | Accepted |
