# ADR 0006: Coding standard: Google C++ Style Guide and C++ Core Guidelines

- Status: Accepted
- Date: 2026-11-14
- Issue: #56
- Amends: ADR 0001 (the C++ version: C++23, not C++20)

## Context

BuCu had a formatter and a naming rule, but no stated design standard, and nothing checked the code against one. Every command from #11 to #15 is written against whatever standard exists when it is written, and a public contract held raw integers whose meaning the type did not show (the flags bitmask above all). The standard must be short to state, checkable by a tool, and must not force a mass rename of working code.

## Considered options

- **Google C++ Style Guide for style, C++ Core Guidelines for design, with recorded exceptions (chosen):** both are public and widely known, and clang-tidy has check families for each (`google-*`, `cppcoreguidelines-*`). The exceptions list stays short because the existing code already follows most of Google.
- **Core Guidelines alone:** a design base with no style rules, so naming, comments, and headers stay open to argument.
- **Google alone:** it forbids exceptions and `<filesystem>`, and its design rules lag the language (C++17 and C++20 are mostly outside it).
- **Keep the current rules and add nothing:** nothing stops drift, and every new command re-opens the same questions.

## Decision

We will take the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) as the style base and the [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines.html) as the design base. Where they disagree, the Core Guidelines decide design and Google decides style. The exceptions to Google are these, and no others:

| Google rule | Google's reason | BuCu's choice | BuCu's reason |
| --- | --- | --- | --- |
| No C++ exceptions | Existing code is not exception-safe, and exceptions cost binary size and predictability | BuCu code never throws for an expected failure; it returns `std::expected` (architecture §7). Code under `src/cli/` MAY catch an exception that third-party code throws (CLI11 today), and only there. Catch2's `*_THROWS` assertions are allowed in tests | BuCu has no legacy exception-unsafe code, and CLI11 throws on a usage error. Confining `catch` to the `cli` edge keeps the rule checkable: `scripts/tidy.sh` fails on a `throw` in `src/` or `tests/`, and on a `catch` outside `src/cli/` |
| No `<filesystem>` (portability worries) | The guide lists it among the headers it does not allow | `std::filesystem` is used, with paths built from UTF-8 (constitution principle 12) | Every compiler in the CI matrix has it, and it is the portable way to handle paths and Windows UTF-16 |
| C++20 at most | The guide's current baseline | C++23 (`CMAKE_CXX_STANDARD 23`, architecture §2) | `std::expected` carries the typed errors. This amends ADR 0001, which said C++20 |
| `.cc` and `.h`, `#ifndef` header guards | Convention of Google's own code | `.cpp` and `.hpp`, with `#pragma once` | The files and the guards already exist, and every compiler in the matrix supports `#pragma once` |
| `CamelCase` function names | Convention of Google's own code | `snake_case` functions and variables, `PascalCase` types, `kPascalCase` constants (architecture §8) | The existing code and tests use them, and `readability-identifier-naming` enforces them. No mass rename |
| 2-space indent, 80 columns | Convention of Google's own code | 4-space indent and 100 columns (`.clang-format`) | Same reason; clang-format is the authority |
| A licence boilerplate or copyright line at the top of each file | Legal housekeeping | None yet. A per-file SPDX line is a follow-up (#59) | The licence and the upstream notices are kept at the repository level (constitution principle 11) |

Static analysis enforces the standard. `.clang-tidy` enables `google-*`, `cppcoreguidelines-*`, `bugprone-*`, and `performance-*`, and keeps the naming checks. `cppcoreguidelines-avoid-magic-numbers` is disabled until the readability, modernize, and misc checks are enabled together (#58). A new `tidy` job in `ci.yml` runs `scripts/tidy.sh` with clang-tidy 19 on every pull request and fails on any diagnostic. Version 19 or newer is required: version 18 cannot parse `std::expected`. A suppression is a `NOLINT(<check>)` on one line or one function with its reason, and `scripts/tidy.sh` fails on a `NOLINT` that names no check. This adds the gate to constitution §3.

Public contracts name what a value means. `core::Flag` and `core::FlagSet` (the flags), `db::SchemaVersion`, and `platform::TerminalStatus` with `platform::StandardStreams` replace raw integers and booleans, and each public enum has a `std::uint8_t` underlying type. The contract record is [`module-006`](../contracts/module-006-typed-flags-schema-version-terminal.md). A parameter struct for `core::make_bookmark` is #60.

## Consequences

- A contributor reads one standard, and the exceptions and their reasons are in this ADR.
- The first CI run of the gate, and the fixes it needed, are part of #56. Later changes that break an adopted check fail CI and name the check.
- Code under `#ifdef _WIN32` is not analysed, because the job runs on Linux. This is a known gap.
- Third-party code is not analysed (ADR 0004).
- Contributors need clang-tidy 19 or newer to run `scripts/tidy.sh` locally. CI does not depend on that.
- Follow-up work: the broad checks and magic numbers (#58), SPDX lines (#59), and the `make_bookmark` parameters (#60).
