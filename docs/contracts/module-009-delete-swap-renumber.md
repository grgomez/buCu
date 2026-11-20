# Module 009: Delete, swap, and renumber

- Kind: Module
- Version: 9 (from 8)
- Status: Proposed
- Date: 2026-11-20
- Issue: #88
- Previous record: [`module-008`](module-008-id-selection-and-bookmark-updates.md)
- ADR: none

## Summary

`core` gains `parse_single_id`, which reads one ID and refuses a list or a range. `db` gains five repository functions: one that checks that a selection is stored, and four that delete bookmarks, swap two IDs, count the IDs out of place, and renumber them. This record changes the contracts of [`module-002`](module-002-core-domain.md) (`core`) and [`module-004`](module-004-db-bookmarks-and-maintenance.md) (`db`), as extended by [`module-008`](module-008-id-selection-and-bookmark-updates.md).

## Module contract

- **Contract:** the single-ID parser of `core`, and the delete, ID swap, and renumber functions of `db`.
- **Owner:** `core` (`include/bucu/core/id_selection.hpp`) and `db` (`include/bucu/db/bookmarks.hpp`).
- **Callers:** `cli` (`delete`, `swap`, `reorder`) and the tests.
- **Inputs:**
  - `core::parse_single_id(std::string_view)`: one argument, read by the digit rules of `parse_id_selection`.
  - `db::check_bookmarks_exist(Database&, const core::IdSelection&)` and `db::delete_bookmarks(Database&, const core::IdSelection&)`.
  - `db::swap_bookmark_ids(Database&, core::BookmarkId a, core::BookmarkId b)`, with `a` and `b` different.
  - `db::count_misplaced_ids(Database&)` and `db::renumber_bookmarks(Database&)`.
  - The new enum `core::SingleIdError` uses `std::uint8_t`.
- **Outputs:**
  - `core::parse_single_id -> std::expected<core::BookmarkId, core::SingleIdError>`.
  - `db::check_bookmarks_exist -> std::expected<void, Error>`: one read transaction, changes nothing.
  - `db::delete_bookmarks -> std::expected<std::int64_t, Error>`: one `BEGIN IMMEDIATE` transaction, one `DELETE` for each covered range, deletes the unused tags before the commit, and returns the number of bookmarks deleted. No other bookmark changes its ID.
  - `db::swap_bookmark_ids -> std::expected<void, Error>`: one `BEGIN IMMEDIATE` transaction; both IDs change in `bookmark` and `bookmark_tag` with deferred foreign key checks; every other column stays on its row.
  - `db::count_misplaced_ids -> std::expected<std::int64_t, Error>`: one read transaction; the number of bookmarks whose ID is not their place in ascending ID order.
  - `db::renumber_bookmarks -> std::expected<std::int64_t, Error>`: one `BEGIN IMMEDIATE` transaction; the stored bookmarks get the IDs 1 to n in their current order, and the result is the number of bookmarks whose ID changed.
  - Every new function is `[[nodiscard]]`.
- **Errors:**
  - `core::SingleIdError::kNotAnIdItem` (not an ID item) and `kListOrRange` (a valid list or range, `3,3` and `3-3` included).
  - `db::delete_bookmarks` returns `kNotFound` as `update_bookmarks` does, before it deletes anything. `db::check_bookmarks_exist` returns the same error. `db::swap_bookmark_ids` returns `kNotFound` with `bookmark_id` set to the lower ID that is not stored.
  - `cli` maps `kNotFound` to exit code `5` (`docs/product.md` §6).
- **Tests:** `tests/core/id_selection_test.cpp` (`parse_single_id` cases) and `tests/db/bookmarks_test.cpp` (`delete_bookmarks`, `check_bookmarks_exist`, `swap_bookmark_ids`, `count_misplaced_ids`, and `renumber_bookmarks` cases).
- **Change from the previous record:** adds the types and functions above. Nothing is removed or renamed.
