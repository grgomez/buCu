# BuCu

BuCu is a C++ port of Buku, originally by Arun Prakash Jana: a command-line bookmark manager with a private, portable database. [Buku](https://github.com/jarun/buku) is the original Python program; BuCu is an independent port, not an official Buku release.

Buku's Python source stays available in this repository's git history and at the upstream Buku project.

## Licence and attribution

BuCu is free software under the GNU General Public License, version 3 or (at your option) any later version (GPL-3.0-or-later). The full text is in [`LICENSE`](LICENSE). Copyright © 2015-2026 Arun Prakash Jana and the Buku contributors; see [`NOTICE`](NOTICE) for the notice BuCu preserves.

## Build

BuCu needs a C++ compiler and the libraries listed in [architecture §2](docs/architecture.md#2-stack). The build commands arrive with [#4](https://github.com/grgomez/buCu/issues/4).

## Roadmap

Each issue owns its scope; the issue state is the source of truth.

### MVP

- [#4](https://github.com/grgomez/buCu/issues/4) CMake skeleton and `bucu --version` / `--help`
- [#5](https://github.com/grgomez/buCu/issues/5) CI build and test matrix
- [#6](https://github.com/grgomez/buCu/issues/6) SQLite schema v1 and forward-only migrations
- [#7](https://github.com/grgomez/buCu/issues/7) Benchmark harness skeleton and baseline
- [#8](https://github.com/grgomez/buCu/issues/8) Platform layer: data directory, console, and terminal
- [#9](https://github.com/grgomez/buCu/issues/9) Core domain: bookmark model, URLs, and tags
- [#10](https://github.com/grgomez/buCu/issues/10) `add` and `db` commands
- [#11](https://github.com/grgomez/buCu/issues/11) `list` command
- [#12](https://github.com/grgomez/buCu/issues/12) `search` command
- [#13](https://github.com/grgomez/buCu/issues/13) `update`, `delete`, and `tag` commands
- [#14](https://github.com/grgomez/buCu/issues/14) Editor and interactive add/update
- [#15](https://github.com/grgomez/buCu/issues/15) `open` command
- [#16](https://github.com/grgomez/buCu/issues/16) Title and description fetch with libcurl
- [#17](https://github.com/grgomez/buCu/issues/17) HTML (Netscape) bookmark import and export
- [#18](https://github.com/grgomez/buCu/issues/18) Markdown import and export

### Later

- [#19](https://github.com/grgomez/buCu/issues/19) Shell completion
- [#20](https://github.com/grgomez/buCu/issues/20) Full benchmark suite
- [#21](https://github.com/grgomez/buCu/issues/21) Measured performance pass
- [#22](https://github.com/grgomez/buCu/issues/22) Release packaging and GPL source checklist
- [#23](https://github.com/grgomez/buCu/issues/23) Import an existing Buku database
- [#24](https://github.com/grgomez/buCu/issues/24) Browser bookmark auto-detection and import
- [#25](https://github.com/grgomez/buCu/issues/25) Org and XBEL import and export
- [#26](https://github.com/grgomez/buCu/issues/26) Wayback Machine integration
- [#27](https://github.com/grgomez/buCu/issues/27) Database encryption
- [#28](https://github.com/grgomez/buCu/issues/28) Server and HTTP API
- [#29](https://github.com/grgomez/buCu/issues/29) Optional Buku CLI compatibility mode
- [#30](https://github.com/grgomez/buCu/issues/30) BSD build in CI
- [#31](https://github.com/grgomez/buCu/issues/31) Merging databases between machines

## Setup (development)

To work on BuCu, turn on the repository's git hooks once per clone:

```sh
git config core.hooksPath .githooks
```

The hooks check each commit subject, the formatting of staged C++, and the branch name on push. The hooks need Python 3 as `python3`, or as `python` on Windows. C++ changes also need `clang-format`.
