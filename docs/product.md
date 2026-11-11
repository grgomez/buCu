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
| Flag | A per-bookmark on/off property (added by #9) |
| Database | The single SQLite file that holds every bookmark. Location: §3 |
| Offline | A run in which BuCu makes no network request: `--offline`, or a build without network support (constitution principle 8) |
| Interactive | A run whose standard input and output are both terminals. Prompts and pagers appear only in interactive runs |

## 3. Database location (added by #8)

- The database lives in the user data directory of the platform, unless `--db <path>` or the `BUCU_DB` environment variable names another file. `--db` wins over `BUCU_DB`.
- Directory names per platform and the creation rule: settled by #8.

## 4. Bookmark rules (added by #9)

URL validation and normalization, tag rules, and flags. Settled by #9.

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

- Output to a non-terminal MUST be plain text with no colour and no pager, so it works in pipes and scripts.
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
