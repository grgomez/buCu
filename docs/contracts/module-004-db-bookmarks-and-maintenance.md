# Module 004: The db module's bookmarks and maintenance

- Kind: Module
- Version: 4 (from 3)
- Status: Accepted
- Date: 2026-11-13
- Issue: #10
- Previous record: [`module-003`](module-003-platform.md)
- ADR: none

## Summary

The first repository operations of the `db` module: storing a bookmark with its tags, the integrity check, and the online backup. A duplicate URL is a typed error that carries the ID that already holds it. A bookmark ID is the type `core::BookmarkId`, and the time passed to a write is `std::chrono::sys_seconds`.

## Module contract

- **Contract:** the bookmark storage and database maintenance operations (`include/bucu/db/bookmarks.hpp`, `maintenance.hpp`, and the additions to `error.hpp`), in namespace `bucu::db`, and `include/bucu/core/bookmark_id.hpp`, in namespace `bucu::core`. The rules are in `docs/product.md` §4, §5 (`add`, `db`), and §6, and `docs/database-schema.md`.
- **Owner:** the `db` module.
- **Callers:** `cli` (`add`, `db check`, `db backup`).
- **Inputs:** `insert_bookmark(Database&, const core::Bookmark&, std::chrono::sys_seconds now)`, where the bookmark is already validated and `now` is stored as `created_at` and `updated_at` in Unix seconds, UTC; `integrity_check(Database&)`; `backup(Database&, const std::filesystem::path&)`, where the path is a new file.
- **Outputs:** `insert_bookmark` returns the new ID in `std::expected<core::BookmarkId, Error>`, after storing the bookmark and its tags in one transaction. `integrity_check` returns `std::expected<std::vector<std::string>, Error>`: the problems SQLite reports as it words them, empty when the database is intact; a failure part way is the last line. `backup` returns `std::expected<void, Error>` and leaves a consistent copy of what was committed when it ran, even while another connection writes.
- **Errors:** `ErrorCode` gains `kDuplicateUrl` (nothing changed; `Error::bookmark_id` holds the existing ID) and `kDestinationExists` (the backup file exists and is not touched). A missing parent directory of the backup file is `kCannotOpen`. A failed backup removes the new file and its `-journal`, `-wal`, and `-shm` files. `Error` gains `std::optional<core::BookmarkId> bookmark_id`, empty for every other code.
- **Bookmark ID:** `core::BookmarkId` has an `explicit` constructor from `std::int64_t`, `value()`, and defaulted comparison (`operator<=>`), no default constructor, no arithmetic, and no implicit conversion to or from `std::int64_t`. Only `src/db/bookmarks.cpp` (bind and read) and the `cli` record and duplicate message unwrap it.
- **Tests:** `tests/core/bookmark_id_test.cpp`, `tests/db/bookmarks_test.cpp`, and `tests/db/maintenance_test.cpp`.
- **Change from the previous record:** `insert_bookmark`, `integrity_check`, `backup`, and `core::BookmarkId` are new; `ErrorCode` has two new codes and `Error` has a third member. No existing signature changed.
