# ADR 0010: libFuzzer for the core text parsers

- Status: Accepted
- Date: 2026-11-16
- Issue: #66

## Context

The `core` functions that take untrusted text (UTF-8 validation and decoding, lowercasing, URL normalization, tag parsing) must never crash, read out of bounds, or hit undefined behaviour, whatever bytes arrive. Example tests and properties ([ADR 0009](0009-rapidcheck-property-tests.md)) cover only inputs someone thought of or generated from a model. A fuzzer finds the rest by coverage feedback. The build must stay offline ([ADR 0004](0004-vendored-dependencies.md)), and `core` must not depend on a test tool.

## Considered options

- **libFuzzer, from the compiler's runtime (chosen):** ships with LLVM Clang, links into the target with `-fsanitize=fuzzer`, and works with the existing ASan and UBSan flags. Nothing is vendored. It runs only under LLVM Clang.
- **AFL++:** works with more compilers, but needs a separate install and its own instrumenting compiler, and it would be a new tool to package for CI.
- **Vendor a fuzzer:** keeps the build self-contained, but libFuzzer is part of LLVM and a vendored copy would drift from the compiler.

## Decision

We will use libFuzzer, the fuzzing engine of LLVM compiler-rt. Its licence is Apache-2.0 WITH LLVM-exception. It is installed with the compiler (the `libclang-rt-19-dev` package that `clang-19` recommends on Ubuntu 24.04, the same package that holds the ASan runtime CI already uses). BuCu does not vendor, ship, or link it into any product binary.

Three targets live in `tests/fuzz/`: `utf8_fuzz`, `url_fuzz`, and `tags_fuzz`. A CMake option `BUCU_FUZZ` (default `OFF`) builds them and stops configuration unless the compiler is LLVM Clang. The `fuzz` preset instruments the whole build with ASan, UBSan, and `fuzzer-no-link`. Each target aborts on a sanitizer report and on a broken invariant of the function contracts. Seed corpora in `tests/fuzz/corpus/<target>/` are replayed by ctest as `fuzz-<target>`, so a seed that once crashed stays a regression test. `scripts/fuzz.sh [seconds]` is the one entry for local and CI runs.

The workflow `fuzz.yml` runs 60 s per target on every pull request, and 600 s per target weekly and on demand. Fuzzing runs on Linux with LLVM Clang only.

## Consequences

- A parser that mishandles any byte sequence is found by the PR run or the weekly run, and the crashing input is uploaded.
- Apple Clang and MSVC cannot build the targets; the normal build and tests are unchanged on every platform.
- A seed added to the corpus fails every PR until the bug is fixed. That is the intent.
- A new `core` text parser gets a target in `tests/fuzz/` in the same change.
- The weekly schedule is proven only after merge. The PR run uses the same script and job, so only the trigger and the budget differ.
