# Architecture decision records

Read this index in every session. Read an ADR only when the issue touches its decision. Start a new ADR from [`0000-template.md`](0000-template.md), and add its row here in the same commit. When an ADR supersedes or amends another, update both rows.

| ADR | Decision | Status |
| --- | --- | --- |
| [0001](0001-cpp20-cmake-sqlite.md) | C++20, CMake, and SQLite for a native, portable port of Buku | Accepted |
| [0002](0002-module-layout.md) | Six library modules: core, db, io, net, platform, cli | Accepted |
| [0003](0003-plan-mode-sdd-workflow.md) | SDD with Claude Code plan mode as the approval gate; specs live in the issue body | Accepted |
| [0004](0004-vendored-dependencies.md) | Vendor SQLite, CLI11, and Catch2 in `third_party/` | Accepted |
| [0005](0005-libcurl-networking.md) | libcurl for the optional title and description fetch | Accepted |
