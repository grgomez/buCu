# Module 009: Queries for `list`

- Kind: Module
- Version: 9 (from 8)
- Status: Accepted
- Date: 2026-11-20
- Issue: #11
- Previous record: [`module-008`](module-008-id-selection-and-bookmark-updates.md) (`core`, `db`); for `platform`, [`module-005`](module-005-platform-untranslated-output.md)
- ADR: none

## Summary

`list` needs the digit rule of an ID for `--last`, a not-found check before it prints, a query for the last N bookmarks, and, on Windows, a console that shows colour. `core` exposes `parse_id`; `db` checks named IDs in `for_each_bookmark` and gains `for_each_last_bookmark`; `platform::ConsoleSetup` turns on escape-sequence processing.

## Module contract

- **Contract:** the ID digit rule of `core`, the bookmark stream of `db`, and the console setup of `platform`.
- **Owner:** `core` (`include/bucu/core/id_selection.hpp`), `db` (`include/bucu/db/bookmarks.hpp`), and `platform` (`include/bucu/platform/console.hpp`).
- **Callers:** `cli` (`list`, and `update` and `tag` through `for_each_bookmark`) and the tests.
- **Inputs:**
  - `core::parse_id(std::string_view)`: ASCII digits only (no sign, no spaces), leading zeros allowed, at least 1, fits a signed 64-bit integer.
  - `db::for_each_bookmark(Database&, const core::IdSelection&, const BookmarkVisitor&)`: unchanged.
  - `db::for_each_last_bookmark(Database&, std::int64_t count, const BookmarkVisitor&)`: `count` below 1 visits nothing.
  - `platform::ConsoleSetup`: unchanged; it gains two private members: the console mode it replaced and whether it changed it.
- **Outputs:**
  - `core::parse_id` returns `std::optional<std::int64_t>` (no value for anything else), `[[nodiscard]]`. It was internal to `id_selection.cpp` and is now declared in the header; `parse_id_selection` uses it as before.
  - `db::for_each_last_bookmark` returns `std::expected<void, Error>`, `[[nodiscard]]`: visits the `count` stored bookmarks with the highest IDs, with their tags, in ascending ID order, one row at a time, in one read transaction for both statements (the lowest ID, then the walk); it visits all of them when fewer than `count` are stored, and stops without error when the visitor returns `false`. It counts stored bookmarks, not ID values.
  - `platform::ConsoleSetup`, on Windows when standard output is a console: turns on `ENABLE_VIRTUAL_TERMINAL_PROCESSING` for standard output, and puts the previous console mode back when destroyed or when Ctrl+C or Ctrl+Break ends the process. On failure, output still goes out. It does nothing on other platforms or when standard output is not a console.
- **Errors:**
  - `db::for_each_bookmark` now returns `ErrorCode::kNotFound`, before the first visit and in the same read transaction, when a selection that names IDs has a single ID that is not stored or a range that holds no stored ID, naming the first such item as `update_bookmarks` does (`bookmark_id` or `range`). A selection of every bookmark has no check, so an empty database visits nothing and succeeds. Before this record, a missing ID was skipped.
  - A caller that reads after a write (`update`, `tag`) can now get `kNotFound` when another process deletes a target between the write and the read; `cli` reports it as a failed print, exit `1` (docs/product.md §6).
  - `cli` maps `kNotFound` to exit code `5` for `list` (`docs/product.md` §6).
- **Tests:** `tests/core/id_selection_test.cpp` (`parse_id`), `tests/db/bookmarks_test.cpp` (`for_each_bookmark` not-found cases and `for_each_last_bookmark` cases), and `tests/platform/console_test.cpp` (the console mode, skipped when no console is attached).
- **Change from the previous record:** `parse_id` is public; `for_each_bookmark` fails with `kNotFound` for a named ID that is not stored (was: skipped); `for_each_last_bookmark` is added; `ConsoleSetup` enables and restores escape-sequence processing on Windows. Nothing is removed or renamed.
