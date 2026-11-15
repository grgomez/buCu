# Module 007: Results of public functions are not discarded

- Kind: Module
- Version: 7 (from 6)
- Status: Accepted
- Date: 2026-11-14
- Issue: #58
- Previous record: [`module-006`](module-006-typed-flags-schema-version-terminal.md)
- ADR: [ADR 0007](../adr/0007-readability-modernize-misc-checks.md)

## Summary

Seven public functions whose result is the whole point of the call are marked `[[nodiscard]]`, so a caller that drops the result gets a compiler warning, an error with warnings as errors. No CLI behaviour, signature, or stored value changes.

## Module contract

- **Contract:** the `[[nodiscard]]` attribute on the functions below. The rule is the clang-tidy check `modernize-use-nodiscard` (`docs/architecture.md` §8).
- **Owner:** `core` for `Flag::bits`, `FlagSet::bits`, and `BookmarkId::value` (`include/bucu/core/bookmark.hpp`, `bookmark_id.hpp`); `db` for `Database::schema_version` and `SchemaVersion::value` (`include/bucu/db/database.hpp`, `error.hpp`); `platform` for `Environment::get` and `SystemEnvironment::get` (`include/bucu/platform/environment.hpp`).
- **Callers:** `db`, `cli`, and the tests. None discards a result, so no caller changes.
- **Inputs:** unchanged.
- **Outputs:** unchanged.
- **Errors:** none added.
- **Tests:** none new. The build with warnings as errors proves that no caller discards a marked result.
- **Change from the previous record:** the seven functions above gain `[[nodiscard]]`; nothing else changes.
