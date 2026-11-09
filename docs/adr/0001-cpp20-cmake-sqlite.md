# ADR 0001: C++20, CMake, and SQLite

- Status: Accepted
- Date: 2026-11-09
- Issue: #1

## Context

BuCu ports Buku, a Python bookmark manager, to get lower startup latency and memory use, and to run on Linux, macOS, Windows, and ideally BSD from one codebase. Bookmarks need durable, local, queryable storage in one portable file.

## Considered options

- **C++20 with CMake and SQLite (chosen):** native speed, mature compilers on every target, CMake as the common build for GCC, Clang, and MSVC, and SQLite as the most portable embedded database.
- **Rust with Cargo:** comparable speed and better memory safety, but the owner chose C++ for this port.
- **C++ with Meson or Bazel:** good tools, but weaker MSVC and IDE support than CMake, and fewer contributors know them.

## Decision

We will write BuCu in C++20, using C++23 features only when every CI compiler supports them and they improve clarity or portability. We will build with CMake and presets, and store data in SQLite.

## Consequences

- Memory safety rests on RAII, the coding standards (architecture §8), and sanitizer builds in CI.
- MSVC lags on some C++23 features; the CI matrix decides what is usable.
- The schema is ours to design; importing Buku's database maps it (#23).
