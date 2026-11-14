# Module 005: Untranslated standard output on Windows

- Kind: Module
- Version: 5 (from 4)
- Status: Accepted
- Date: 2026-11-13
- Issue: #10
- Previous record: [`module-004`](module-004-db-bookmarks-and-maintenance.md)
- ADR: none

## Summary

`ConsoleSetup` also puts standard output and standard error in untranslated (binary) mode on Windows, so every line BuCu writes ends in LF on every platform.

## Module contract

- **Contract:** the console guard (`include/bucu/platform/console.hpp`), in namespace `bucu::platform`. The rule is in `docs/product.md` §5.
- **Owner:** the `platform` module.
- **Callers:** `cli` (`main` holds `ConsoleSetup`).
- **Inputs:** none; the constructor takes no argument.
- **Outputs:** on Windows, while the object lives, standard output and standard error are untranslated, with or without a console attached, and the previous modes are restored when it is destroyed. On other platforms nothing changes.
- **Errors:** none. A stream that is not open is left as it is.
- **Tests:** `tests/cli/lf_output.cmake`, registered as the `cli_lf_output` test, runs the built `bucu` with redirected output and fails on a CR byte; the Windows job of CI is where it can fail.
- **Change from the previous record:** the behaviour of `ConsoleSetup` on Windows; the class has two new private members and no new public member. `module-003` described the UTF-8 code page only.
