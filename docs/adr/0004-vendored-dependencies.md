# ADR 0004: Vendored dependencies

- Status: Accepted
- Date: 2026-11-09
- Issue: #1

## Context

BuCu builds with GCC, Clang, and MSVC, and must build without an internet connection once its sources are present. Development happens on Linux, while CI covers macOS and Windows, where system packages for SQLite and test libraries are absent or vary.

## Considered options

- **Vendor in `third_party/` (chosen):** same versions everywhere, offline by default, no package manager; costs repository size and manual updates.
- **CMake FetchContent:** smaller repository, but the first build needs the network.
- **vcpkg or Conan:** strong on Windows, but every contributor and CI job needs the tool.
- **System packages only:** lightest, but Windows needs extra setup and SQLite features vary.

## Decision

We will vendor SQLite (amalgamation), CLI11, and Catch2 (amalgamated) in `third_party/<name>/`, each with its licence and a `VERSION` file, refreshed by `scripts/update-deps.sh`. `BUCU_USE_SYSTEM_SQLITE=ON` builds against the system SQLite for distribution packagers. libcurl is the exception: it is too large to vendor, and its source per platform is settled by #16 ([ADR 0005](0005-libcurl-networking.md)).

## Consequences

- Updates are deliberate commits that can be reviewed.
- The licences of vendored code ship with the source and the binaries (#22).
