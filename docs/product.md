# Product Rules

The behaviour of the `bucu` command-line program: what each command does, what it prints, and how it fails. How it is built: `docs/architecture.md`. When to read this document and how it changes: `CLAUDE.md`.

Normative words (MUST, SHOULD, MAY): [constitution §6](constitution.md#6-normative-language).

Sections marked "(added by #N)" describe a part that issue #N builds. Until that issue merges, the section states the agreed direction, and the issue's Spec part settles the details.

---

## 1. Purpose and users

BuCu is a fast, portable command-line bookmark manager, a C++ port of Buku by Arun Prakash Jana. It serves people who keep bookmarks in a terminal-friendly, local database and want quick startup, low memory use, and scripting-friendly output. Everything is stored locally in one SQLite file; nothing is synchronized to a cloud.

The first milestone is a robust core, not full Buku compatibility. Buku compatibility grows incrementally (`README.md` Roadmap).

## 2. Glossary

| Term | Meaning |
| --- | --- |
| Bookmark | One saved URL with a title, a description, tags, and flags. Identified by its ID |
| ID | The positive integer that identifies a bookmark in commands. Stability across deletes: §5 (added by #13) |
| Tag | A short label attached to bookmarks. A bookmark has zero or more distinct tags. Case and character rules: §4 (added by #9) |
| Flag | A per-bookmark on/off property. The MVP has one: immutable title (§4) |
| Database | The single SQLite file that holds every bookmark. Location: §3 |
| Offline | A run in which BuCu makes no network request: `--offline`, or a build without network support (constitution principle 8) |
| Interactive | A run whose standard input and output are both terminals. Prompts and pagers appear only in interactive runs |

## 3. Database location (added by #8)

- The database lives in the user data directory of the platform, unless `--db <path>` or the `BUCU_DB` environment variable names another file. `--db` wins over `BUCU_DB`.
- Without an override, the database is `bookmarks.db` in a `bucu` directory under the platform data directory:
  - Linux and BSD: `$XDG_DATA_HOME`, when it is an absolute path; otherwise `$HOME/.local/share`.
  - macOS: `$HOME/Library/Application Support`.
  - Windows: `%APPDATA%`.
- An explicit path (`--db`) wins over `BUCU_DB`, and `BUCU_DB` wins over the default. An empty value counts as absent. A relative override is relative to the current directory.
- BuCu creates the default directory and its parents when they are missing. It creates no directory for an override: the override's parent directory must exist.
- When the location cannot be determined or prepared, BuCu reports an error that names the variable (`HOME` or `APPDATA` unset or empty, with a suggestion to use `--db` or `BUCU_DB`) or the directory (missing, or a file is in its place). An override, or an absolute `XDG_DATA_HOME` on Linux and BSD, needs no `HOME`.

## 4. Bookmark rules (added by #9)

Rules for the URL, tags, flags, and text of a bookmark. Each is checked in `core` before anything is stored, and a broken rule is a validation error that names the field (URL, tag, title, or description) and the reason; §6 describes how a command prints it. Commands use these rules from #10 on.

Definitions used below:

- Whitespace is the Unicode `White_Space` property: U+0009 to U+000D, U+0020, U+0085, U+00A0, U+1680, U+2000 to U+200A, U+2028, U+2029, U+202F, U+205F, and U+3000.
- A control character is general category Cc: U+0000 to U+001F and U+007F to U+009F.
- Unicode data is version 18.0.0. A newer version needs a new plan (`scripts/update-deps.sh`).
- All text MUST be valid UTF-8; BuCu refuses anything else with a validation error that names the field.

### URL

- A URL MUST be trimmed of leading and trailing whitespace, then be valid UTF-8 and have an RFC 3986 scheme followed by `:` and a non-empty rest. It MUST contain no whitespace and no control character.
- A URL has no scheme, and BuCu refuses it, in two cases. The text after its first `:` starts with one or more digits followed by the end or `/`: a host and port (`localhost:8080`, `example.org:8080/x`, as Buku tests it). The part before the first `:` is one letter: a drive letter (`C:\Users\a.html`). This test runs before the scheme grammar. A text with no `:` (`example.org`) has no scheme either.
- Any other absolute URI is valid, so entries from browser exports are kept: `about:blank`, `place:sort=8`, `mailto:a@example.org`, `file:///home/a.html`. A scheme list is not used, because real bookmark files hold many that no list has (`place:`, `javascript:`, `moz-extension:`, `edge:`).
- A URL with an authority (`//` after the scheme) MUST have a non-empty host, except for the `file` scheme.
- Normalization MUST lowercase the scheme and the host (with the Unicode lowercase mapping of the tag rules below, so `https://ÉXAMPLE.org` becomes `https://éxample.org/`), remove the port when its digits equal the default of the scheme (`http` 80, `https` 443, `ftp` 21), and use `/` as the path when a URL with an authority has an empty path. The `/` goes right after the authority, before any `?` or `#`.
- Normalization MUST NOT change anything else: user information, path (including a trailing `/`), query, and fragment stay as typed. It does no IDNA (punycode) conversion, no percent-encoding change, and no dot-segment removal. `https://example.org/docs` and `https://example.org/docs/` stay different.
- The authority ends at the first `/`, `?`, or `#`. User information is the part before the last `@`. The host is in `[...]` for IPv6, with its hex digits lowercased and the brackets kept. The port follows the last `:` outside the brackets; a port that is not the default, or is malformed (`:abc`), is kept as typed.
- Two bookmarks MUST NOT share a normalized URL, compared byte for byte. `HTTPS://Example.ORG`, `https://example.org:443`, and `https://example.org/` are one bookmark. The `UNIQUE` column in the database enforces it (`docs/database-schema.md` §3).

### Tags

- Tag input MUST be split on commas. Each tag is trimmed, each run of inner whitespace becomes one space (so a tab or newline inside a tag becomes a space), and an empty tag is dropped. Input of only commas and whitespace is an empty set, not an error.
- Each tag MUST be lowercased with the Unicode full lowercase mapping, with no language tailoring: the per-character mapping, U+0130 as U+0069 U+0307, and the Final_Sigma rule (`ΣΑΣ` becomes `σας`). The result equals Python's `str.lower()`, which Buku uses. No Unicode normalization (NFC) is applied: this is a known limit, and Buku has it too.
- A tag MUST contain no control character and MUST have at most 128 characters (Unicode code points), counted after lowercasing. Otherwise BuCu returns a validation error that names the tag.
- A bookmark's tags MUST be a set: no duplicates, in ascending byte order of the normalized tag.
- Tag operations MUST be add (union), remove (difference; removing a tag the bookmark does not have is not an error), and replace (the new set, which may be empty).

### Flags

- Flags are a bitmask. The MVP has one, immutable title, with value `1`, the same bit as Buku. A new bookmark has no flag set. Changing one flag MUST keep every other bit, including bits BuCu does not know.
- What the flag does when a title is fetched: #16.

### Title and description

- Title and description MUST be valid UTF-8, are stored as given (newlines and leading or trailing whitespace included), and have no length limit. Each may be empty.

## 5. Commands

The MVP command interface. Each command's options, output, and errors are settled by the issue named.

```text
bucu [--db <path>] [--color auto|always|never] [--offline] <command> [args]
bucu --version | -h | --help

add <url> [tags...]        add a bookmark; prompts or opens the editor when asked (#10, #14, #16)
list                       list bookmarks, paginated (#11)
search <query>             search by keyword, substring, or tag (#12)
search --regex <pattern>   search by regular expression (#12)
tag <id> <operation>       add, remove, or replace tags (#13)
update <id>                change a bookmark's fields (#13, #14)
delete <id>                delete bookmarks, with confirmation (#13)
open <id-or-query>         open bookmarks in the system browser (#15)
export <format> <file>     export to html or md (#17, #18)
import <format> <file>     import from html or md (#17, #18)
db <path|version|check|backup>   database utilities (#10)
completion <shell>         print a shell completion script (#19)
```

- Output to a non-terminal MUST be plain text with no pager, and with no colour unless `--color always` is given, so it works in pipes and scripts.
- `--color auto` (the default) uses colour only when standard output is a terminal and `NO_COLOR` is unset or empty. `--color always` uses colour even when output is not a terminal or `NO_COLOR` is set. `--color never` uses no colour. Standard error does not change the choice.
- A command that would prompt in an interactive run MUST NOT wait for input in a non-interactive run; it fails with a message that names the option to use instead (for example `--yes`).

## 6. Messages and exit codes (added by #10)

- Errors go to standard error, start with `bucu: `, and say what failed and what to do. Normal output goes to standard output.
- Exit codes: `0` success, `1` a runtime failure, `2` a usage error. Further codes (for example "not found"): settled by #10.
- A bare `bucu`, or `bucu --` with nothing after it, prints the help text to standard error and exits `2`.
- `--help` and `-h` print the help text, and `--version` prints the version, whichever other arguments are given; both exit `0`. When both are given, the help text wins, in either order.
- A usage error prints one line, `bucu: <what failed>. Run 'bucu --help' for usage.`, to standard error, nothing to standard output, and exits `2`.
- When writing standard output fails, `bucu` prints `bucu: cannot write to standard output` to standard error and exits `1`.

## 7. Import and export formats (added by #17, #18)

HTML (Netscape bookmark file) and Markdown. Shapes and duplicate handling: settled by #17 and #18.

## 8. Network fetch (added by #16)

Title and description fetch on `add`. Default, timeouts, and limits: settled by #16.
