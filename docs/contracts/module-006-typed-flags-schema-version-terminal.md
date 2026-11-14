# Module 006: Typed flags, schema version, and terminal status

- Kind: Module
- Version: 6 (from 5)
- Status: Proposed
- Date: 2026-11-14
- Issue: #56
- Previous record: [`module-005`](module-005-platform-untranslated-output.md)
- ADR: [ADR 0006](../adr/0006-coding-standard-google-and-core-guidelines.md)

## Summary

Four raw values in public headers become types that say what they mean: the bookmark flags (`core`), the schema version (`db`), and the terminal status of standard input and output (`platform`). Each public enum also gets a `std::uint8_t` underlying type. No CLI behaviour and no stored value changes.

## Module contract

- **Contract:** the typed values below, in namespaces `bucu::core`, `bucu::db`, and `bucu::platform`. The rules are in `docs/product.md` §4 (Flags) and `docs/database-schema.md` §2.
- **Owner:** `core` for `Flag` and `FlagSet` (`include/bucu/core/bookmark.hpp`); `db` for `SchemaVersion` (`include/bucu/db/error.hpp`, `database.hpp`); `platform` for `TerminalStatus` and `StandardStreams` (`include/bucu/platform/terminal.hpp`).
- **Callers:** `db` (stores `FlagSet::bits()`), `cli` (all three), and the tests.
- **Inputs:**
  - `Flag(std::int64_t)` and `FlagSet(std::int64_t)`: explicit only. `Bookmark::flags` is a `FlagSet`; `has_flag`, `set_flag`, and `clear_flag` take `(FlagSet, Flag)`. A `Flag` converts implicitly to a `FlagSet` of that flag. `kFlagImmutableTitle` is a `Flag`.
  - `SchemaVersion(int)`: explicit only. `kSchemaVersion` is a `SchemaVersion`.
  - `is_interactive` takes one `StandardStreams { TerminalStatus input; TerminalStatus output; }`, and `use_color` takes the `TerminalStatus` of standard output.
- **Outputs:** `FlagSet::bits()` is the stored integer, bit for bit, with unknown bits and the highest bit kept. `Database::schema_version()` returns a `SchemaVersion`, read with `value()`. `stdin_is_terminal()` and `stdout_is_terminal()` return a `TerminalStatus`.
- **Enum sizes:** `core::Field`, `core::Reason`, `db::ErrorCode`, `platform::ErrorCode`, `platform::Os`, `platform::ColorMode`, and `platform::TerminalStatus` have `std::uint8_t` as their underlying type.
- **Errors:** none added.
- **Tests:**
  - `tests/core/bookmark_test.cpp`: the conversions (`static_assert`), unknown bits, and the highest bit.
  - `tests/db/bookmarks_test.cpp`: the flags stored bit for bit.
  - `tests/db/database_test.cpp`: the `SchemaVersion` conversions (`static_assert`).
  - `tests/platform/terminal_test.cpp`: the `TerminalStatus` conversions (`static_assert`) and `is_interactive`.
  - `tests/core/error_test.cpp`, `tests/db/error_test.cpp`, `tests/platform/error_test.cpp`: the enum sizes.
- **Change from the previous record:** `Bookmark::flags`, `has_flag`, `set_flag`, `clear_flag`, `kFlagImmutableTitle`, `kSchemaVersion`, `Database::schema_version()`, `use_color`, `is_interactive`, `stdin_is_terminal`, and `stdout_is_terminal` change type; `Flag`, `FlagSet`, `SchemaVersion`, `TerminalStatus`, and `StandardStreams` are new; seven enums gain an underlying type.
