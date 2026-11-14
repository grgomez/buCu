# Module 004: The db module's bookmarks and maintenance

- Kind: Module
- Version: 4 (from 3)
- Status: Accepted
- Date: 2026-11-13
- Issue: #10
- Previous record: [`module-003`](module-003-platform.md)
- ADR: none

## Summary

The first repository operations of the `db` module: storing a bookmark with its tags, the integrity check, and the online backup. A duplicate URL is a typed error that carries the ID that already holds it.

## Module contract

- **Contract:** the bookmark storage and database maintenance operations (`include/bucu/db/bookmarks.hpp`, `maintenance.hpp`, and the additions to `error.hpp`), in namespace `bucu::db`. The rules are in `docs/product.md` §4, §5 (`add`, `db`), and §6, and `docs/database-schema.md`.
- **Owner:** the `db` module.
- **Callers:** `cli` (`add`, `db check`, `db backup`).
- **Inputs:** `insert_bookmark(Database&, const core::Bookmark&, std::int64_t now)`, where the bookmark is already validated and `now` is Unix seconds, UTC; `integrity_check(Database&)`; `backup(Database&, const std::filesystem::path&)`, where the path is a new file.
- **Outputs:** `insert_bookmark` returns the new ID in `std::expected<std::int64_t, Error>`, after storing the bookmark and its tags in one transaction. `integrity_check` returns `std::expected<std::vector<std::string>, Error>`: the problems SQLite reports as it words them, empty when the database is intact; a failure part way is the last line. `backup` returns `std::expected<void, Error>` and leaves a consistent copy of what was committed when it ran, even while another connection writes.
- **Errors:** `ErrorCode` gains `kDuplicateUrl` (nothing changed; `Error::bookmark_id` holds the existing ID) and `kDestinationExists` (the backup file exists and is not touched). A missing parent directory of the backup file is `kCannotOpen`. A failed backup removes the new file and its `-journal`, `-wal`, and `-shm` files. `Error` gains `std::optional<std::int64_t> bookmark_id`, empty for every other code.
- **Tests:** `tests/db/bookmarks_test.cpp` and `tests/db/maintenance_test.cpp`.
- **Change from the previous record:** `insert_bookmark`, `integrity_check`, and `backup` are new; `ErrorCode` has two new codes and `Error` has a third member. No existing signature changed.
