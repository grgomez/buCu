# Architecture and Development Guide

## 1. Purpose

This document is the architectural contract for BuCu: its stack, modules, contracts, data flow, and coding, testing, and portability standards. CLI behaviour is in `docs/product.md`; stored data in `docs/database-schema.md`. When to read this document and how it changes: `CLAUDE.md`.

Normative words (MUST, SHOULD, MAY): [constitution §6](constitution.md#6-normative-language).

Sections marked "(added by #N)" describe a part that issue #N builds. Until that issue merges, the section states the agreed design, and the issue's plan can refine it.

---

## 2. Stack

| Dependency | Role | Licence | Why |
| --- | --- | --- | --- |
| C++23; minimum compilers GCC 13 and Clang 19 (Apple Clang 17 from Xcode 16.4, MSVC 19.51) | Language | — | Native speed and startup; `std::expected` for typed errors (§7) |
| CMake ≥ 3.25 with `CMakePresets.json` | Build | — | The common denominator for the three toolchains; presets give one command per build type |
| SQLite amalgamation, vendored in `third_party/sqlite/` (FTS5 on) | Storage | Public domain | One known version and feature set on every platform. `BUCU_USE_SYSTEM_SQLITE=ON` links the system library instead, for distribution packagers ([ADR 0004](adr/0004-vendored-dependencies.md)) |
| CLI11, vendored | Argument parsing with subcommands | BSD-3-Clause | Header-only, subcommand support, generated help, no other dependency |
| Unicode Character Database casing tables, generated into `third_party/unicode/` | Tag lowercasing in `core` without ICU | Unicode License V3 | Buku lowercases tags with Python's `str.lower()`; a generated table gives the same result on every platform with no dependency. `scripts/update-deps.sh` pins the source and generated files by SHA256 |
| libcurl, system library; Windows source settled by #16 | Optional title and description fetch (`src/net/`) | curl (MIT-style) | The portable HTTP and TLS client with timeouts and size limits; behind `BUCU_WITH_NET` ([ADR 0005](adr/0005-libcurl-networking.md)) |

Test and quality tools:

| Tool | Role | Why |
| --- | --- | --- |
| Catch2 v3, vendored amalgamated | Unit, integration, and golden-file tests; micro-benchmarks | Two files to vendor; sections, generators, and matchers; built-in benchmarking. Chosen over doctest (fewer matchers, no benchmarks) and GoogleTest (heavier to vendor) |
| RapidCheck, vendored at a pinned commit | Property tests of the pure `core` functions, with generators and shrinking | BSD-2-Clause. No tagged releases, so `scripts/update-deps.sh` pins a commit archive by SHA256; our own CMake file builds it, because the upstream one forces C++11 and changes global flags ([ADR 0009](adr/0009-rapidcheck-property-tests.md)) |
| libFuzzer | Fuzz targets of the `core` text parsers, run under ASan and UBSan | Apache-2.0 WITH LLVM-exception. Part of LLVM compiler-rt, installed with the compiler (`libclang-rt-19-dev`); nothing is vendored or shipped. LLVM Clang on Linux only ([ADR 0010](adr/0010-libfuzzer-fuzz-targets.md)) |
| ctest | Test runner across platforms | Ships with CMake |
| clang-format | Formatting authority (`.clang-format`) | |
| ccache | Compiler cache in CI only; not linked into BuCu | Keeps the matrix fast; local builds stay free of it |
| clang-tidy 19 or newer | Static analysis (`.clang-tidy`): `google-*`, `cppcoreguidelines-*`, `bugprone-*`, `performance-*`, and the naming checks | Run by `scripts/tidy.sh` and the CI `tidy` job. Version 18 cannot parse `std::expected` ([ADR 0006](adr/0006-coding-standard-google-and-core-guidelines.md)) |
| AddressSanitizer, UndefinedBehaviorSanitizer, ThreadSanitizer | Memory, undefined-behaviour, and data-race checks | Presets `asan-ubsan` and `tsan`, where the compiler supports them |
| llvm-cov and llvm-profdata (LLVM 19 or newer) | Source-based coverage report (`coverage` preset, `scripts/coverage.sh`) | Apache-2.0 WITH LLVM-exception. Installed with LLVM (`llvm-19`); not shipped. Linux and macOS, clang only ([ADR 0011](adr/0011-coverage-and-crap-report.md)) |
| lizard 1.26.1 | Cyclomatic complexity for the CRAP score | MIT. A Python tool, not vendored; CI installs the pinned version with `pipx`, developers with `uv tool install lizard==1.26.1`. `scripts/coverage.sh` names it and exits 2 when it is missing ([ADR 0011](adr/0011-coverage-and-crap-report.md)) |
| hyperfine (optional) | Command-line benchmarks | `scripts/bench.sh` falls back to a portable timer |

Each vendored dependency lives in `third_party/<name>/` with its licence file and a `VERSION.txt` file, and `scripts/update-deps.sh` refreshes it. No other dependency is allowed without a plan decision (constitution §4). The choice of this stack: [ADR 0001](adr/0001-cpp20-cmake-sqlite.md).

Platforms: Linux x86-64 (GCC, Clang), macOS ARM64 (Apple Clang), Windows x86-64 (MSVC). BSD is a goal (#30). The CI matrix: `.github/workflows/ci.yml`. The issue that adds the first thread adds the `tsan` entry.

---

## 3. High-level architecture

```mermaid
flowchart TD
    cli["cli<br/>CLI11 parsing, output formatting, exit codes, wiring"] --> io["io<br/>HTML and Markdown import and export"]
    cli --> db["db<br/>SQLite, migrations, transactions, statements"]
    cli --> core["core<br/>bookmark model, URLs, tags, search queries, validation"]
    io --> db
    io --> core
    db --> core
    cli --> platform["platform<br/>paths, browser, editor, terminal"]
    cli --> net["net<br/>optional title and description fetch"]
```

`cli` creates the platform and network implementations and passes them, as interfaces, to the code that needs them. Module structure: [ADR 0002](adr/0002-module-layout.md).

---

## 4. Module responsibilities

Each module is a CMake library target under `src/<module>/`, with its public headers in `include/bucu/<module>/`. The target graph enforces the direction in §3. A module's target is created with its first source file.

| Module | Owns | MUST NOT |
| --- | --- | --- |
| `core` | Bookmark model, URL validation and normalization, tag parsing and operations, ID selection parsing (IDs, ranges, and every bookmark), the rules for checking and applying changes to a bookmark, search query parsing, validation errors | Include SQLite, CLI11, libcurl, or any operating-system header; read the clock or the environment (receive them as parameters) |
| `db` | Connection setup, schema migrations, prepared statements, transactions, repositories that stream rows, integrity check and backup | Format output; parse command lines; contain rules that belong in `core` |
| `io` | Import and export formats over streams, one transaction per import | Load a whole file or result set into memory; open files by itself (receive streams) |
| `net` | The fetch interface and the libcurl implementation, timeouts, size caps | Be required by any other module; run unless asked (constitution principle 8) |
| `platform` | Data directory, browser launch, editor launch, terminal detection and colour, UTF-8 console, untranslated (LF) standard output | Contain business rules |
| `cli` | Subcommands, options, output formatting, pagination, exit codes, wiring of the implementations | Run SQL directly; contain rules that belong in `core` |

Moving a responsibility needs an ADR (constitution §4).

### 4.1 Repository layout

```text
CMakeLists.txt, CMakePresets.json
include/bucu/<module>/  # public headers (module contracts, §5)
src/<module>/           # module sources
tests/                  # Catch2 tests: unit, integration, golden files; fuzz targets and seed corpus in tests/fuzz/
benchmarks/             # datasets, harness, results (docs/performance.md)
third_party/            # vendored dependencies with licences (§2)
scripts/                # format, bench, update-deps, coverage
docs/                   # living documents
docs/adr/, docs/contracts/
tools/                  # SDD workflow tools (CLAUDE.md Commands); not part of BuCu
.claude/                # agent skills and hooks (SDD workflow); not part of BuCu
.githooks/              # git hooks, enabled with core.hooksPath; not part of BuCu
.github/workflows/      # CI
```

---

## 5. Key contracts

Each contract has three parts: its rules (linked below); one record for each version change in [`docs/contracts/`](contracts/), never edited after merge, which is its history; and the code or files that hold its exact current shape. Start a record from [`0000-template.md`](contracts/0000-template.md).

| Contract | Rules | Records | Exact shape |
| --- | --- | --- | --- |
| Database schema | [`docs/database-schema.md`](database-schema.md) | `db-NNN` (number = `PRAGMA user_version`) | `src/db/migrations.cpp` |
| Export formats | product §7 | `export-NNN` | One sample file per format version, read by the import tests (added by #17) |
| Module contracts | §4 | `module-NNN` | The headers in `include/bucu/` and the tests that lock them |

---

## 6. Data flow

```mermaid
flowchart LR
    args[argv] --> cli[cli: parse] --> core[core: validate, normalize] --> db[db: statement, transaction] --> sqlite[(SQLite)]
    sqlite -->|row by row| db --> cli -->|format| out[stdout]
```

- Queries stream rows to the formatter one at a time; no command holds a whole result set.
- Domain rules receive plain values and return plain values or errors.

---

## 7. Error handling

- Expected failures (invalid input, a missing bookmark, a malformed import file, a constraint violation) MUST return a typed error (`std::expected`) that `cli` maps to a message and an exit code (product §6).
- A duplicate URL is a typed `db` error (`kDuplicateUrl`) that carries the ID of the bookmark that already holds the URL, so `cli` can name it (product §6).
- An ID that is not stored is a typed `db` error (`kNotFound`) that carries the missing single ID or the range with no stored ID, and neither when no bookmark exists, so `cli` can name it (product §6).
- An import validates as it streams, inside one transaction; a failed import changes no data.
- Writes that change more than one table run in one transaction.
- BuCu code MUST NOT throw an exception for an expected failure: no `throw` in `src/` or `tests/` (a test helper that fails calls Catch2 `FAIL`; Catch2's `*_THROWS` assertions are allowed). Only `src/cli/` MAY catch, and only around third-party code that throws (CLI11 today). `scripts/tidy.sh` enforces this.
- Do not catch `...` or `std::exception` without handling it. Broad silent handlers are prohibited.

---

## 8. Coding standards

- **Standard:** the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) is the style base and the [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines.html) are the design base. Each exception to Google, with its reason, is in [ADR 0006](adr/0006-coding-standard-google-and-core-guidelines.md).
- **Formatting:** clang-format is the authority. **Analysis:** clang-tidy with the checks in `.clang-tidy`, run by `scripts/tidy.sh` and by the CI `tidy` job, which fails on any diagnostic.
- **Warnings:** every BuCu target builds with the strict set, and warnings are errors in every preset and in CI (`BUCU_WERROR`, `ON` in every preset, `OFF` by default in `CMakeLists.txt` so a packager's newer compiler does not break the build).
  - GCC and Clang: `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast -Wnon-virtual-dtor -Woverloaded-virtual -Wcast-align -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough`.
  - GCC only: `-Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast`.
  - MSVC: `/W4 /WX /permissive- /utf-8` (source and execution character sets are UTF-8), with `/external:anglebrackets /external:W0` for third-party headers.
- **Third-party code:** vendored headers (CLI11, Catch2) are included as `SYSTEM`; vendored sources (the SQLite amalgamation) build in their own target without the strict set. The strict set never weakens for BuCu's own code, tests included. The generated Unicode tables (`third_party/unicode/*.inc`) are analysed through their include in `src/core/unicode_lower.cpp`, so each carries the `NOLINT` pair that `scripts/update-deps.sh` writes ([ADR 0007](adr/0007-readability-modernize-misc-checks.md)).
- **Suppressions:** silence a warning only at the narrowest scope (one line or one function, never a file or target, except the `NOLINTBEGIN`/`NOLINTEND` pair that `scripts/update-deps.sh` writes into a generated `third_party/` table, with a reason line), with a comment that names the warning and why the code is correct. Do not cast just to quiet a warning. A clang-tidy check is disabled in `.clang-tidy` only with a comment that gives the reason and the issue that will enable it, or, for a check that will never be enabled, the issue that decided the exclusion; a single finding is suppressed with `NOLINT(<check>)` on one line or one function, with its reason, and `scripts/tidy.sh` fails on a `NOLINT` that names no check. For `readability-magic-numbers` and `cppcoreguidelines-avoid-magic-numbers`, name a constant instead of suppressing; a `NOLINT` for either is allowed only on one line, with a reason that a name cannot fix, and never as a `NOLINTBEGIN`/`NOLINTEND` pair (`scripts/tidy.sh` fails on such a pair).
- **Ownership:** RAII for every resource (SQLite handles, files, processes). No owning raw pointers. `std::string_view` where ownership is not needed, and never stored past the life of its source.
- **Hot paths:** avoid needless allocations; measure before adding caches, custom allocators, or complex structures (`docs/performance.md`).
- **Text:** UTF-8 in `std::string` everywhere. Convert to UTF-16 only inside `src/platform/` on Windows.
- **Platform code:** `#ifdef` on the operating system only in `src/platform/` (constitution principle 9).
- **Naming:** `snake_case` functions and variables, `PascalCase` types, `kPascalCase` constants, namespaces `bucu::<module>`. `.clang-tidy` enforces them.
- **Comments:** when a code comment or test name cites a source, it cites an issue, a living-document section, or an ADR, never a task or criterion ID from an issue body.

---

## 9. Testing standards

| Layer | Tests |
| --- | --- |
| `core` | Unit tests for every rule in product §4, including error paths |
| `db` | Integration tests on temporary database files: migrations from each version, CRUD, transactions, cascades |
| `io` | Round-trip tests and malformed-input tests with fixtures written for BuCu |
| `cli` | Argument handling, exit codes, and golden-file tests of stable output |
| `platform`, `net` | Tests through fakes of the interfaces; no real browser, editor, or network in CI |

- Platform rules take the operating system and the environment as parameters, so each operating system's rule is tested on every CI platform. Tests reach operating-system calls only through `src/platform/test_support.hpp` (internal, not in `include/`), so no test holds an operating-system `#ifdef`.
- Tests MUST NOT depend on the machine's clock, locale, time zone, home directory, or network. Inject them.
- Every bug fix has a regression test (constitution §3).
- Test behaviour, not implementation details.
- `core` properties take their rules from product §4 and cite the rule in the test name. ctest runs them as `core-property` with a fixed seed; run `bucu_core_property_tests` with another `RC_PARAMS` (for example `seed=7 max_success=2000`) to explore more inputs.
- Fuzz targets for the `core` text parsers live in `tests/fuzz/` with seed corpora in `tests/fuzz/corpus/<target>/` ([ADR 0010](adr/0010-libfuzzer-fuzz-targets.md)). They build only with `BUCU_FUZZ`, which the `fuzz` preset sets (LLVM Clang, Linux). ctest replays each corpus as `fuzz-<target>`; `scripts/fuzz.sh [seconds]` fuzzes each target for the time and exits non-zero on a finding.
- The `coverage` preset (clang, Linux and macOS) and `scripts/coverage.sh` report line, region, branch, and function coverage and a CRAP score for each function of `src/` and `include/` ([ADR 0011](adr/0011-coverage-and-crap-report.md)). The CI `coverage` job runs them on every PR and is informational: it fails on a broken tool, never on a threshold.

Each CI job checks something no other check does. BuCu has no CI Debug job: `src/` and `include/` have no `assert()` and no `NDEBUG`, so Release runs the same tests with more warnings. The local `debug` gate of constitution §3 is BuCu's Debug run; Release's `NDEBUG` turns off only the vendored libraries' asserts, which the local `debug` gate still runs.

| Job | Workflow | What only it checks |
| --- | --- | --- |
| `ubuntu-gcc-13-release` | `ci.yml` | GCC's optimization-only warnings as errors, and libstdc++ bounds and precondition checks (`-D_GLIBCXX_ASSERTIONS`) |
| `ubuntu-clang-19-release` | `ci.yml` | The tests on the Clang 19 Release build without sanitizers or extra library checks, as shipped |
| `ubuntu-clang-19-asan-ubsan` | `ci.yml` | Memory errors and undefined behaviour at run time |
| `macos-15-apple-clang-release` | `ci.yml` | Apple Clang and libc++ on macOS |
| `windows-msvc-release` | `ci.yml` | MSVC and the Windows platform code |
| `format` | `ci.yml` | clang-format conformance |
| `tidy` | `ci.yml` | clang-tidy diagnostics on the `release` build |
| `coverage` | `ci.yml` | Line, branch, and CRAP reports from an instrumented `-O0` build, and the report scripts' tests; informational |
| `fuzz` | `fuzz.yml` | libFuzzer runs of the `core` parsers, and clang-tidy on the fuzz targets |
| `gates` | `sdd-gates.yml` | The issue body and pull request rules of constitution §3 |
| `tools-tests` | `sdd-gates.yml` | The workflow tools' unit tests |
