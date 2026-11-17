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

- **Contract:** `std::expected<Bookmark, ValidationError> make_bookmark(const BookmarkFields& fields)` in `include/bucu/core/bookmark.hpp`. `BookmarkFields` is an aggregate of four `std::string_view` members, in the order `url`, `title`, `description`, `tags`. Callers use designated initializers: `make_bookmark({.url = ..., .title = ..., .description = ...})`.
- **Owner:** `core`.
- **Callers:** `cli` (`src/cli/add.cpp`) and the tests, all moved to the named form. `db` does not call it.
- **Inputs:** `url`, `title`, and `description` are required. `tags` is optional: leaving it out, or naming it empty, means no tags. The views are read during the call only; the bookmark owns copies of its text.
- **Outputs:** unchanged.
- **Errors:** unchanged. The same `ValidationError` is returned, naming the same field, and fields are checked in the same order (URL, title, description, tags).
- **Tests:** `tests/core/bookmark_test.cpp` (named values reach their fields, tags left out or empty, check order); `core-bookmark-fields-required-<field>` ctest entries (`tests/core/CMakeLists.txt`, `tests/core/bookmark_fields_required.cpp`) prove that leaving out `url`, `title`, or `description` does not build.
- **Limits:**
  - A call that leaves out a required field fails to build because the strict warning set (`-Wextra`, warnings as errors, architecture §8) rejects the missing designated initializer on GCC and Clang. With warnings not as errors (`BUCU_WERROR=OFF` in a packager build) the compiler only warns, and the field is empty. MSVC may accept the omission. The ctest entries run only on GCC and Clang (Apple Clang included).
  - `tags` carries a default member initializer, with a one-line `NOLINT(readability-redundant-member-init)`: the initializer is what exempts `tags` from the missing-field warning. Removing it as redundant makes a call that leaves out `tags` fail to build.
- **Change from the previous record:** the four positional parameters of `make_bookmark` become one `const BookmarkFields&`; nothing else changes.
