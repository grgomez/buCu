# Module 003: The platform layer

- Kind: Module
- Version: 3 (from 2)
- Status: Accepted
- Date: 2026-11-13
- Issue: #8
- Previous record: [`module-002`](module-002-core-domain.md)
- ADR: none

## Summary

The first public headers of the `platform` module: environment access, the database location rules, terminal and colour detection, and the UTF-8 console guard. `platform` depends on no other BuCu module.

## Module contract

- **Contract:** the platform rules (`include/bucu/platform/error.hpp`, `environment.hpp`, `paths.hpp`, `terminal.hpp`, `console.hpp`), in namespace `bucu::platform`. The rules are in `docs/product.md` §3 and §5.
- **Owner:** the `platform` module.
- **Callers:** `cli` (`main` holds `ConsoleSetup`); `--db` and `--color` are wired from #10.
- **Inputs:** `Environment::get(name)` (UTF-8 text; `SystemEnvironment` reads the process environment, through UTF-16 on Windows); `resolve_database_location(Os, Environment, explicit_path)`, with `Os` one of `kLinuxOrBsd`, `kMacOs`, `kWindows`; `prepare_database_location(Location)`; `use_color(ColorMode, stdout_is_terminal, Environment)` with `ColorMode` one of `kAuto`, `kAlways`, `kNever`; `is_interactive(stdin_is_terminal, stdout_is_terminal)`; `path_from_utf8` and `utf8_from_path`. `current_os()`, `stdin_is_terminal()`, and `stdout_is_terminal()` read the real system.
- **Outputs:** `std::expected<Location, Error>` (UTF-8 path text joined with the separator of the given `Os`, and whether it is the default); `std::expected<std::filesystem::path, Error>` (the path of the database file, with its default directory created); `bool` for colour and interactivity.
- **Errors:** `Error` holds an `ErrorCode` (`kMissingVariable`, `kMissingDirectory`, `kCannotCreateDirectory`) and a message that names the variable or directory.
- **Console:** `ConsoleSetup` selects the UTF-8 console output code page on Windows, restores the previous one when destroyed or on Ctrl+C or Ctrl+Break, and does nothing elsewhere.
- **Tests:** `tests/platform/paths_test.cpp`, `environment_test.cpp`, `terminal_test.cpp`, and `console_test.cpp`.
- **Change from the previous record:** first version of the `platform` headers.
