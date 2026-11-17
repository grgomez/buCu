# Module 008: ID selection and bookmark updates

- Kind: Module
- Version: 8 (from 7)
- Status: Accepted
- Date: 2026-11-17
- Issue: #87
- Previous record: [`module-007`](module-007-nodiscard-public-results.md)
- ADR: none

## Summary

`core` gains the ID selection that `update` and `tag` read (and `delete` will reuse), and the rules that check and apply a change to a bookmark. `db` gains three repository functions that count, stream, and update bookmarks, and a typed not-found error. This record changes the contracts of [`module-002`](module-002-core-domain.md) (`core`) and [`module-004`](module-004-db-bookmarks-and-maintenance.md) (`db`).

## Module contract

- **Contract:** the ID selection and change rules of `core`, and the bookmark count, stream, and update of `db`.
- **Owner:** `core` (`include/bucu/core/id_selection.hpp`, `changes.hpp`, `bookmark.hpp`) and `db` (`include/bucu/db/bookmarks.hpp`, `error.hpp`).
- **Callers:** `cli` (`update`, `tag`) and the tests; `db` calls `core::covered_ranges`, `core::named_id_count`, and `core::apply_changes`.
- **Inputs:**
  - `core::parse_id_selection(const std::vector<std::string>&)`: the ID arguments. Each is ASCII digits (a positive whole number that fits in 64 bits), a range `<a>-<b>`, or a comma-separated list of these; no argument means every bookmark.
  - `core::make_changes(const ChangeRequest&)`: optional URL, title, description, a `TagChange` (`TagOperation` and raw tag text), and a `FlagChange` (`kSet` or `kClear`).
  - `db::update_bookmarks(Database&, const core::IdSelection&, const core::BookmarkChanges&, std::chrono::sys_seconds now)`.
  - `db::for_each_bookmark(Database&, const core::IdSelection&, const BookmarkVisitor&)`, where `BookmarkVisitor` is `std::function<bool(core::BookmarkId, const core::Bookmark&)>`.
  - The new enums (`IdItemKind`, `TagOperation`, `FlagChange`) use `std::uint8_t`.
- **Outputs:**
  - `core::IdSelection` (`all`, and `items` sorted by low end, a single ID before a range, no item twice); `core::IdItem`, `IdRange`, `IdError` (the whole argument); `core::covered_ranges` (ascending ranges that neither overlap nor touch; one range from 1 to the largest ID for every bookmark); `core::named_id_count` (distinct IDs named, 0 for every bookmark).
  - `core::BookmarkChanges` (the checked request: normalized URL, valid texts, parsed tags); `core::apply_changes(Bookmark, const BookmarkChanges&) -> Bookmark`; a defaulted `Bookmark::operator==`.
  - `db::count_bookmarks -> std::expected<std::int64_t, Error>`; `db::for_each_bookmark -> std::expected<void, Error>`: visits the stored bookmarks of the selection, with their tags, in ascending ID order in one read transaction, skips an ID that is not stored, and stops without error when the visitor returns `false`; `db::update_bookmarks -> std::expected<void, Error>`: one `BEGIN IMMEDIATE` transaction, writes only a bookmark whose values change (`updated_at = now`, `created_at` kept), and deletes tags that no bookmark links before the commit.
  - Every new function is `[[nodiscard]]`.
- **Errors:**
  - `core::make_changes` returns the first broken rule as a `ValidationError`, checked in the order URL, title, description, tags.
  - New `db::ErrorCode::kNotFound` and `db::Error::range` (`std::optional<core::IdRange>`). `update_bookmarks` returns `kNotFound` for the first item, in ascending order of its low end, that matches no stored bookmark: a single ID sets `bookmark_id`, a range sets `range`, and for every bookmark with none stored neither is set. That check comes before `kDuplicateUrl`.
  - `cli` maps `kNotFound` to exit code `5` (`docs/product.md` §6).
- **Tests:** `tests/core/id_selection_test.cpp`, `tests/core/changes_test.cpp`, `tests/fuzz/id_selection_fuzz.cpp`, and `tests/db/bookmarks_test.cpp` (`count_bookmarks`, `for_each_bookmark`, and `update_bookmarks` cases).
- **Change from the previous record:** adds the types and functions above, `ErrorCode::kNotFound`, `Error::range`, and `Bookmark::operator==`. `bucu_db` links `bucu_core`. Nothing is removed or renamed.
