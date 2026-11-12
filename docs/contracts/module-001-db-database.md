# Module 001: The db module's Database

- Kind: Module
- Version: 1 (from 0)
- Status: Proposed
- Date: 2026-11-11
- Issue: #6
- Previous record: none
- ADR: none

## Summary

The first public headers of the `db` module: `bucu::db::Database`, which opens or creates a BuCu database, and `bucu::db::Error`. Prepared statements and transactions stay internal to `src/db/`.

## Module contract

- **Contract:** the database connection (`include/bucu/db/database.hpp`, `include/bucu/db/error.hpp`).
- **Owner:** the `db` module.
- **Callers:** none yet; `cli` opens the database from #8 and #10.
- **Inputs:** `Database::open(const std::filesystem::path&)`: a path, UTF-8 safe; a missing file is created.
- **Outputs:** `std::expected<Database, Error>`. `Database` is a move-only owner of one open connection, with foreign keys on, WAL where the file system supports it, a 5-second busy timeout, and `schema_version()` equal to `kSchemaVersion` (1). The headers do not include `sqlite3.h`.
- **Errors:** `Error` has an `ErrorCode` (`kCannotOpen`, `kNotADatabase`, `kNotBucuDatabase`, `kNewerSchema`, `kMigrationFailed`, `kSql`) and an English message that names the cause. A refusal leaves the file's bytes unchanged. `cli` maps codes to messages and exit codes (#10).
- **Tests:** `tests/db/database_test.cpp` and `tests/db/migration_test.cpp`.
- **Change from the previous record:** first version.
