# Module 008: make_bookmark takes named fields

- Kind: Module
- Version: 8 (from 7)
- Status: Accepted
- Date: 2026-11-16
- Issue: #60
- Previous record: [`module-007`](module-007-nodiscard-public-results.md)
- ADR: [ADR 0006](../adr/0006-coding-standard-google-and-core-guidelines.md)

## Summary

`core::make_bookmark` takes one `BookmarkFields` in place of four adjacent `std::string_view` parameters, so each call names the URL, title, description, and raw tag text it passes, and two of them cannot be swapped unnoticed (Core Guidelines I.4, `bugprone-easily-swappable-parameters`). No CLI behaviour, message, exit code, or stored value changes.

## Module contract

- **Contract:** `std::expected<Bookmark, ValidationError> make_bookmark(const BookmarkFields& fields)` in `include/bucu/core/bookmark.hpp`. `BookmarkFields` is an aggregate of four members, in the order `url`, `title`, `description`, `tags`: the first three are `RequiredText` and `tags` is a `std::string_view`. `RequiredText` (same header) has one public `std::string_view value`, an `explicit` constructor from `std::string_view`, and no default constructor. Callers use designated initializers and wrap each required value: `make_bookmark({.url = core::RequiredText{...}, .title = core::RequiredText{...}, .description = core::RequiredText{...}})`.
- **Owner:** `core`.
- **Callers:** `cli` (`src/cli/add.cpp`) and the tests, all moved to the named form. `db` does not call it.
- **Inputs:** `url`, `title`, and `description` are required. `tags` is optional: leaving it out, or naming it empty, means no tags. The views are read during the call only; the bookmark owns copies of its text.
- **Outputs:** unchanged.
- **Errors:** unchanged. The same `ValidationError` is returned, naming the same field, and fields are checked in the same order (URL, title, description, tags).
- **Tests:** `tests/core/bookmark_test.cpp`: named values reach their fields, tags left out or empty, check order, and `static_assert`s on concepts proving that a `BookmarkFields` without `url`, `title`, or `description` is not constructible (positive controls: the full set, and the set without `tags`).
- **Limits:**
  - Leaving out a required field is ill-formed by language rule on GCC, Clang, Apple Clang, and MSVC, with no warning flag, so it holds in a packager build with `BUCU_WERROR=OFF` too (the TR2 trial of issue #60).
  - MSVC 19.51 alone accepts a plain `std::string_view` for a `RequiredText` member despite the `explicit` constructor (a conformance gap); no test asserts that a plain view is rejected, and GCC and Clang CI reject it.
  - `tags` carries a default member initializer, with a one-line `NOLINT(readability-redundant-member-init)`: the initializer is what exempts `tags` from the missing-field warnings. Removing it as redundant makes a call that leaves out `tags` fail to build.
- **Change from the previous record:** the four positional parameters of `make_bookmark` become one `const BookmarkFields&`, whose required members are `RequiredText`; nothing else changes.
