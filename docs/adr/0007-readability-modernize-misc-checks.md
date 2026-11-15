# ADR 0007: Readability, modernize, and misc checks

- Status: Accepted
- Date: 2026-11-14
- Issue: #58
- Amends: ADR 0006 (the list of enabled checks, the rule for a disabled check, and the suppression scope)

## Context

ADR 0006 started the static analysis gate with `google-*`, `cppcoreguidelines-*`, `bugprone-*`, and `performance-*`, and left the readability, modernize, and misc families and the magic-number check for #58. A trial found 184 findings in BuCu code: 91 magic numbers (each reported under two names), 40 positional initializers, 25 results that a caller could drop, and 28 smaller findings. It also found 2,212 findings in the generated Unicode tables, which are included into `src/core/unicode_lower.cpp`. Each table finding has a note at a BuCu type, so clang-tidy counts it as user code, and no header filter keeps it out. A `NOLINT` is read only in the file of the finding.

## Considered options

- **Enable the three families and exclude six checks, each with its reason (chosen):** the code meets the families, and the six exclusions are checks that would flag correct or idiomatic code.
- **Enable only the checks that find something today:** the list drifts from the families, and new checks in a later clang-tidy are not seen.
- **Keep the families off:** magic numbers and dropped results stay unchecked.

For the generated tables:

- **A `NOLINTBEGIN`/`NOLINTEND` pair inside each table, written by `scripts/update-deps.sh` (chosen):** it is the only place clang-tidy reads, and the script, not a hand edit, owns the file.
- **A header filter or an exclude regex:** tried, and neither keeps the findings out (the notes make them user code).
- **Disable the designated-initializer check:** it would also stop checking BuCu code, where it is useful.

## Decision

We will enable `readability-*`, `modernize-*`, and `misc-*`, and turn the magic-number check back on. We will exclude these six checks, each with its reason in `.clang-tidy`:

| Check | Reason |
| --- | --- |
| `modernize-use-trailing-return-type` | The Google C++ Style Guide uses a leading return type |
| `misc-include-cleaner` | It needs per-library include mappings that BuCu does not keep |
| `readability-identifier-length` | It flags idiomatic short names such as `id`, `db`, and `it` |
| `readability-uppercase-literal-suffix` | The Google C++ Style Guide does not require an uppercase suffix |
| `misc-non-private-member-variables-in-classes` | It flags Catch2 fixtures, which are plain structs by design |
| `readability-function-cognitive-complexity` | It scores the Catch2 macro expansion inside each `TEST_CASE` |

A check that will never be enabled records its reason and the issue that decided the exclusion, not an issue that will enable it. This replaces the ADR 0006 rule that a disabled check names the issue that will enable it.

`scripts/update-deps.sh` writes `NOLINTBEGIN(modernize-use-designated-initializers)` and `NOLINTEND(...)` as the first and last line of each generated table, with a reason line that cites this ADR. This is the only whole-file suppression allowed: it covers a generated `third_party/` table, and a hash check fails if the script's output changes. The check stays on for BuCu code.

Public functions whose result is the point of the call are `[[nodiscard]]`, recorded in [`module-007`](../contracts/module-007-nodiscard-public-results.md).

## Consequences

- New code is checked for magic numbers, positional initializers, dropped results, and the other checks of the three families.
- A real problem in an excluded check goes unflagged. Each reason is written down, and re-enabling a check is a one-line change.
- A future change to the generator that drops the `NOLINT` lines fails the hash check and the gate.
- Code under `#ifdef _WIN32` is still not analysed (ADR 0006). The CI Windows build proves that no caller there drops a `[[nodiscard]]` result.
