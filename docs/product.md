# Product Rules

The behaviour of the `bucu` command-line program: what each command does, what it prints, and how it fails. How it is built: `docs/architecture.md`. When to read this document and how it changes: `CLAUDE.md`.

Normative words (MUST, SHOULD, MAY): [constitution §6](constitution.md#6-normative-language).

Sections marked "(added by #N)" describe a part that issue #N builds. Until that issue merges, the section states the agreed direction, and the issue's Spec part settles the details.

---

## 1. Purpose and users

BuCu is a fast, portable command-line bookmark manager, a C++ port of Buku by Arun Prakash Jana. It serves people who keep bookmarks in a terminal-friendly, local database and want quick startup, low memory use, and scripting-friendly output. Everything is stored locally in one SQLite file; nothing is synchronized to a cloud.

The first milestone is a robust core. The long-term goal is feature parity with Buku, reached incrementally (`README.md` Roadmap).

## 2. Glossary

| Term | Meaning |
| --- | --- |
| Bookmark | One saved URL with a title, a description, tags, and flags. Identified by its ID |
| ID | The positive integer that identifies a bookmark in commands. Stability across deletes: [§5 ID stability](#id-stability-added-by-88) |
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
list [ids...] [--last <n>]   print bookmarks (#11); pagination (#97)
search <query>             search by keyword, substring, or tag (#12)
search --regex <pattern>   search by regular expression (#12)
tag [ids...] <operation> [tags...]   add, remove, or replace tags (#87)
update [ids...]            change bookmarks' fields (#87, #14)
delete [ids...]            delete bookmarks, with confirmation (#88)
swap <id> <id>             exchange the IDs of two bookmarks (#88)
reorder                    renumber the IDs to 1..n, keeping their order (#88)
open <id-or-query>         open bookmarks in the system browser (#15)
export <format> <file>     export to html or md (#17, #18)
import <format> <file>     import from html or md (#17, #18)
db path|version|check|backup <file>   database utilities (#10); <file> is for backup only
completion <shell>         print a shell completion script (#19)
```

- Output to a non-terminal MUST be plain text with no pager, and with no colour unless `--color always` is given, so it works in pipes and scripts.
- `--color auto` (the default) uses colour only when standard output is a terminal and `NO_COLOR` is unset or empty. `--color always` uses colour even when output is not a terminal or `NO_COLOR` is set. `--color never` uses no colour. Standard error does not change the choice.
- A command that would prompt in an interactive run MUST NOT wait for input in a non-interactive run; it fails with a message that names the option to use instead (for example `--yes`).
- Every line `bucu` writes to standard output or standard error MUST end in LF (U+000A) only, on every platform, whether the output goes to a terminal, a file, or a pipe. The one exception is the confirmation prompt of `update`, `tag`, `delete`, and `reorder` (below), which ends without LF because the user's Enter ends it; at end of input, `bucu` writes an LF before `Cancelled.`.
- The global options (`--db`, `--color`, `--offline`) MUST come before the command; after it they are a usage error. `--color` and `--offline` are accepted and checked from #10: `--color` takes its effect from #11 (colour rules under "add") and `--offline` with #16.

### add (added by #10)

`bucu add <url> [tags...]` stores one bookmark and prints its record.

- Options: `--title <text>`, `--description <text>`, `--tag <tags>` (repeatable, one value for each use), and `--immutable` (a switch that sets the immutable-title flag, §4 Flags). A title or description that is absent or empty is stored empty.
- Tags: the words after the URL are joined with single spaces and read as one tag input; each `--tag` value is another. All are split on commas by the rules of §4 Tags, and the stored tags are their union.
- A tag that starts with `-` MUST be given after `--`, or as `--tag=<tag>`.
- The URL, tags, title, and description MUST be validated and normalized by §4 before anything is stored. A broken rule is a validation error (§6). The bookmark and its tags are stored in one transaction.
- When the normalized URL is already stored, `add` changes nothing and fails with the duplicate URL error (§6). Updating an existing bookmark is `bucu update` (#87).
- `add` MUST NOT prompt, open an editor, or use the network in #10 (#14 and #16 add them).
- On success, `add` prints the stored record to standard output:
  - Line 1: the ID, two spaces, and the title; only the ID when the title is empty.
  - Then, each only when its value is not empty and in this order: `url:`, `desc:`, `tags:` (tags joined by `, `), and `flags:` (`immutable-title` for bit 1). The `url:` line is always present.
  - Each of these lines starts with three spaces, and its label is padded with spaces to 7 characters, so every value starts in column 11.
  - A line break inside the title or description continues on a new line indented to the value column (10 spaces).
  - Every command that prints this block (`add`, `update`, `tag`, and `list`) applies the colour and control-character rules below.
  - Colour: a record block is coloured exactly when the `--color` rule above says colour is used; otherwise it holds no escape sequence. The colours are Buku's defaults, as SGR codes: the ID bright cyan (`96`); the title bold bright green (`92;1`); the labels `url:`, `desc:`, `tags:`, and `flags:` bright red (`91`); the URL bright yellow (`93`); the tags bright blue (`94`); the flags value dim (`2`); the description uncoloured. Each colour covers these bytes: the ID colour the ID and the two spaces after it (the ID alone when the title is empty); the title colour the title; the label colour the three-space indent, the label, and its padding; a value colour the whole value, the `, ` between tags included. On a continuation line the value colour covers the ten-space indent and the text. Each coloured part ends with the reset code (`0`) before its line ends, so every line starts uncoloured, and a value that continues on another line repeats its colour on that line. On Windows, BuCu turns on escape-sequence processing for a console standard output while it runs and restores the previous mode.
  - Control characters: every control character (§4) in the title or description except LF is written with the escapes of a validation error value (§6), for example `\x1b`, `\t`, `\r`; LF keeps the continuation-line rule. This holds with and without colour, so a stored escape sequence cannot drive the terminal.

```text
7  Example
   url:   https://example.org/
   desc:  A page
   tags:  docs, tools, web dev
   flags: immutable-title
```

### IDs and ranges (added by #87)

`update`, `tag`, `list` (#11), and `delete` (#88) name the bookmarks they act on with the same ID arguments. `list` never asks for confirmation, and with no ID and no bookmark stored it prints nothing and exits `0` (see "list" below), unlike `update` and `tag`.

- The IDs are one or more arguments, each a positive whole number, a range `<a>-<b>`, or a comma-separated list of these (`3`, `3,5`, `3-7`, `3,5-7`, `3 5-7`). A range with `a > b` is read as `<b>-<a>`. An ID that appears twice counts once.
- An ID is ASCII digits only: no sign and no spaces; leading zeros are allowed (`007` is 7); an empty list item (`3,`, `3,,5`) is not an ID. It MUST fit in a signed 64-bit integer.
- An argument that does not follow these rules (for example `abc`, `0`, `-3`, `1-x`, `99999999999999999999`) is a usage error, exit `2`, that quotes the whole argument (§6).
- No ID means every bookmark. `0` does not mean every bookmark, as it does in Buku.
- Every single ID named MUST be stored. A range covers only the stored IDs in it, and a range that covers none is not found; so is a single ID that is not stored, unlike Buku, which prints `No matching index` and exits `0`. Nothing changes, and the command exits `5` (§6) naming the first item that failed, taking the items in ascending order of their low end (a single ID `n` has low end `n`). A range is named as `<low>-<high>`, after the swap.
- Each run MUST be all or nothing: when any target fails (not found, validation, conflict, a write error), no bookmark changes.
- A huge range such as `1-9999999999` is not expanded: only the stored IDs in it are visited, and records are written one at a time.
- For `update` and `tag` with no ID, the command first prints `Update all <n> bookmarks? [y/N] ` to standard output (the prompt is flushed before BuCu reads) in an interactive run without `--yes`, and proceeds only when the answer is `y` or `yes`, in any case, ignoring spaces around it. Any other answer prints `Cancelled.`, changes nothing, and exits `0`; at end of input BuCu first writes an LF, because no Enter ended the prompt line. In a non-interactive run without `--yes` it does not wait: it fails with the usage error that names `--yes` (§6). With `--yes` it never asks. A list or a range never asks `update` or `tag`. `delete` always asks, by the rules of "delete".
- With no ID and no bookmark stored, the command fails with `bucu: there are no bookmarks.` and exit `5`, before any prompt, even in a non-interactive run without `--yes`.
- On success `update` and `tag` print the stored record of each target bookmark in the `add` format (§5 add), in ascending ID order, with one empty line between records, and exit `0`, whether or not a stored value changed. If writing standard output fails after the change is stored, the change stays and the exit code is `1` (§6).
- Errors are checked in this order: usage (exit `2`), validation (exit `3`), not found (exit `5`), confirmation, conflict (exit `4`).

### update (added by #87)

`bucu update [ids...]` changes fields of the bookmarks that the ID arguments name (IDs and ranges, above) and prints their records.

- Options: `--url <url>`, `--title <text>`, `--description <text>`, `--tag <tags>` (repeatable, one value for each use), `--immutable`, `--no-immutable`, and `--yes` (the confirmation of "IDs and ranges"). At least one option other than `--yes` is required, and `--immutable` with `--no-immutable` is not allowed; both are usage errors, exit `2`, and nothing changes. A later issue may give `update` with no field option a meaning (#14, #16).
- Each given field MUST be validated and normalized by §4 before anything is stored, checked in the order URL, title, description, tags. A field that is not given keeps its stored value.
- An empty `--title` or `--description` stores that field empty. The `--tag` values are split and normalized by §4 Tags, and their union replaces the bookmark's tags; an empty union (for example `--tag ''`) removes every tag. To add or remove single tags, use `tag`.
- `--immutable` sets flag bit 1 and `--no-immutable` clears it, keeping every other bit (§4 Flags).
- A new URL that another bookmark already holds fails with the duplicate URL error (§6) and exit `4`. A bookmark given its own URL again is not a conflict.
- `--url` needs exactly one distinct ID, because two bookmarks cannot share a URL (§4 URL): `5`, `5,5`, `5 5`, and `5-5` are accepted; a list or range that names more than one ID, or no ID, is a usage error, exit `2`. The rule counts the IDs named, not the IDs stored.

### tag (added by #87)

`bucu tag [ids...] <operation> [tags...]` adds, removes, or replaces the tags of the bookmarks that the ID arguments name (IDs and ranges, above) and prints their records.

- The operations are `add`, `remove`, and `replace`, applied as defined in §4 Tags. `replace` with no tags removes every tag; `add` or `remove` with no tags is a usage error, exit `2`.
- The first word that is `add`, `remove`, or `replace` is the operation. The words before it are IDs; the words after it are joined with single spaces and read as one tag input, as in `add`, so a tag may be named like an operation or a command (`bucu tag 7 add remove`, `bucu tag 7 add db`). A tag that starts with `-` MUST be given after `--` (`bucu tag 7 add -- -x`).
- With no operation word, the usage error names the first word that is not an ID item (`'rename' is not a tag operation; use add, remove, or replace`), or says that no operation was given.
- `tag` accepts `--yes`, with the meaning given in "IDs and ranges".
- The other rules (not found, all or nothing, output, order of errors) are those of "IDs and ranges".

### ID stability (added by #88)

- Deleting bookmarks MUST NOT change the ID of any other bookmark.
- A new bookmark MUST get the ID one above the highest stored ID, or 1 when none is stored; so a deleted ID is reused only when no higher ID is stored. This differs from Buku, which moves the last bookmark into the gap, and has no `--retain-order`.
- Only `swap` and `reorder` MUST change a stored bookmark's ID, on request. A bookmark whose ID changes keeps every field, tag, flag, and timestamp; its `updated_at` does not change.

### delete (added by #88)

`bucu delete [ids...]` deletes the bookmarks that the ID arguments name (IDs and ranges, above), or every bookmark when no ID is given, after confirmation.

- It takes the ID arguments of "IDs and ranges", with its rules for parsing, not found (exit `5`, nothing deleted), order of errors, and all or nothing. With no bookmark stored, it fails with `bucu: there are no bookmarks.` and exit `5` before any prompt. `--yes` is the only option.
- With IDs, it prints the record of each bookmark it will delete, in the `add` format, in ascending ID order, with one empty line between records, before it asks. With no ID, it prints no records.
- Every `delete` is confirmed. In an interactive run without `--yes`, after the records and an empty line, it prints `Delete <n> bookmarks? [y/N] ` (`Delete 1 bookmark? [y/N] ` for one; with no ID, `Delete all <n> bookmarks? [y/N] `, and `Delete all 1 bookmark? [y/N] ` for one) and reads the answer as "IDs and ranges" describes. Any answer other than `y` or `yes` prints `Cancelled.`, deletes nothing, and exits `0`. In a run that is not interactive without `--yes`, it fails with the usage error that names `--yes` (§6), exit `2`, before it prints any record. With `--yes` it never asks.
- The deletion is one all-or-nothing change. Tags that no bookmark links any more are deleted with it (`docs/database-schema.md` §3).
- On success it prints `Deleted <n> bookmarks.` (`Deleted 1 bookmark.` for one), after the records and an empty line when records were printed, and exits `0`. The count in the prompt and in the summary is the number of stored bookmarks the arguments cover.
- Output, byte for byte: each record (one empty line between), one empty line, then the prompt (interactive without `--yes`; the user's Enter ends its line), then the summary, with no empty line after the prompt. With `--yes`: records, one empty line, the summary. With no ID: the prompt, then the summary.
- When writing a record fails, nothing is deleted, `bucu: cannot write to standard output` is printed, and the exit code is `1`. When writing later output fails, the deletion stays and the exit code is `1` (§6).
- When another process changes the bookmarks between the prompt and the deletion, `delete` deletes what its arguments cover when it runs; when a named single ID is gone by then, it fails with exit `5` and deletes nothing.

### swap and reorder (added by #88)

`bucu swap <a> <b>` exchanges the IDs of two bookmarks. `bucu reorder` gives the stored bookmarks the IDs 1 to n in their current ascending ID order.

- `swap` exchanges the IDs of the bookmarks `a` and `b` in one all-or-nothing change, without asking. It checks, in this order: exactly two arguments; each argument left to right is one ID by the digit rules of "IDs and ranges" (no list and no range); the two IDs differ; both are stored.
- An argument that is not a valid ID item is the invalid ID error (§6), exit `2`. A wrong number of arguments, or a list or a range (`3,3` and `3-3` included), is the "swap takes two IDs" usage error; two equal IDs (`3 003` included) are the "swap needs two different IDs" usage error (both in §6). Both exit `2`, and nothing changes.
- An ID that is not stored is the not-found error (§6) for the lower such ID, exit `5`, and nothing changes.
- On success `swap` prints the records with IDs `a` and `b` after the change, in the `add` format, in ascending ID order, with one empty line between them, and exits `0`.
- `reorder` is one all-or-nothing change. It takes no argument other than `--yes`; an ID is the "reorder takes no ID" usage error (§6), exit `2`. With no bookmark stored it fails with `bucu: there are no bookmarks.` and exit `5`.
- When every ID is already in place, `reorder` prints `IDs are already 1-<n>.` and exits `0` without asking. Otherwise it asks as `delete` does, with the prompt `Renumber <m> bookmarks? [y/N] ` (`Renumber 1 bookmark? [y/N] `), where m is the number of bookmarks whose ID changes; in a run that is not interactive without `--yes` it fails with the usage error that names `--yes` (§6), exit `2`. On success it prints `Renumbered <m> bookmarks.` (`Renumbered 1 bookmark.`) and exits `0`.

### list (added by #11)

`bucu list [ids...] [--last <n>]` prints bookmarks in the `add` record format (§5 add).

- With ID arguments, it prints the record of each bookmark they cover, by the rules of "IDs and ranges" for parsing, not found (exit `5`, nothing printed to standard output), and order of errors. A not-found item is detected before any record is printed.
- Records are printed in ascending ID order with one empty line between records, and written one at a time as they are read, without holding the whole result. The output is the state at the start of the run (one read transaction), even when another process adds or deletes bookmarks meanwhile.
- With no ID, it prints every bookmark; with no bookmark stored, it prints nothing to standard output or standard error and exits `0`.
- `--last <n>` prints the `n` stored bookmarks with the highest IDs, in ascending ID order; when fewer than `n` are stored, it prints all of them, and with no bookmark stored it prints nothing and exits `0`. It counts stored bookmarks, not ID values: with bookmarks 1 and 100 stored, `--last 2` prints both (Buku, which counts back from the highest ID, would print only 100). `<n>` is a positive whole number by the digit rules of "IDs and ranges" (ASCII digits, leading zeros allowed, fits a signed 64-bit integer, not `0`). `--last` with ID arguments is a usage error, exit `2`, and so is a missing value or a second `--last`.
- When a run has several usage errors, `list` reports them in this order: `--last` with IDs, a bad `--last` value, a bad ID.
- `list` does not ask for confirmation, open a pager, or change the database, and accepts no option other than `--last`. Pagination, ordering, and a pager are #97, wrapping to the terminal width #98, JSON output and field selection #99, configurable colours #49.
- When writing standard output fails, the rule of §6 applies: exit `1`.

### db (added by #10)

- `bucu db path` prints the absolute path of the database that the run would use (§3), followed by LF. It does not open or create the database, or any directory, so it can print a path that does not exist yet.
- `bucu db version` opens the database like any command (creating it when missing) and prints `schema <n> (newest supported: <m>)`.
- `bucu db check` runs SQLite's full integrity check. An intact database prints `ok` and exits `0`. Otherwise the report goes to standard output as SQLite gives it, each problem ending in LF (a problem may hold line breaks), then `bucu: the database failed its integrity check. Restore a backup, or export what is readable.` goes to standard error, and the exit code is `1`.
- `bucu db backup <file>` writes a consistent copy of the database to the new file `<file>`, even while another process writes to the database; the copy holds what was committed when the backup ran. It refuses an existing `<file>` (the database's own path included), creates no missing parent directory, and leaves no partial file when it fails. On success it prints `Backed up to <absolute path>.` and `For a portable copy, use 'bucu export'.` (export arrives with #17 and #18). Restoring a backup is not a command: use the copy as the database with `--db` or `BUCU_DB`.
- Every `db` subcommand except `path` opens the database like any command. A file that is not a SQLite database, or has a schema newer than BuCu supports, fails with exit code `1` and an error that names the file.

## 6. Messages and exit codes (added by #10)

- Errors go to standard error, start with `bucu: `, and say what failed and what to do. Normal output goes to standard output.
- Exit codes:

  | Code | Meaning |
  | --- | --- |
  | `0` | Success |
  | `1` | A runtime failure: the database cannot be opened, a backup fails, or writing fails |
  | `2` | A usage error |
  | `3` | A validation error: a rule of §4 is broken |
  | `4` | A conflict: the URL is already stored |
  | `5` | Not found: no stored bookmark matches the IDs given |

  Codes `6` and above are reserved for later issues.
- A bare `bucu`, or `bucu --` with nothing after it, prints the help text to standard error and exits `2`.
- `--help` and `-h` print the help text, and `--version` prints the version, whichever other arguments are given; both exit `0`. When both are given, the help text wins, in either order.
- A usage error prints one line, `bucu: <what failed>. Run 'bucu --help' for usage.`, to standard error, nothing to standard output, and exits `2`.
- A validation error prints one line, `bucu: invalid <field> '<value>': <reason>.`, and exits `3`. The field is `URL`, `tag`, `title`, or `description`. The reason is one of: `it is empty`, `it is not valid UTF-8`, `it has no scheme, such as https://`, `it contains whitespace`, `it contains a control character`, `it has no host`, or `it is longer than 128 characters`. The value is left out for a title, a description, and text that is not valid UTF-8; otherwise it is shown with each control character written as an escape (`\n`, `\x1b`), so the error stays one line. Nothing is stored and nothing goes to standard output.
- A URL that is already stored prints `bucu: <normalized URL> is already bookmarked as <ID>. Use 'bucu update <ID>' to change it.` to standard error, prints nothing to standard output, changes nothing, and exits `4`.
- An ID argument that is not a positive whole number prints `bucu: invalid ID '<argument>': it is not a positive whole number. Run 'bucu --help' for usage.` and exits `2`. The argument is shown as given, with each control character written as an escape, as for a validation error.
- A `list --last` value that is not a positive whole number prints `bucu: invalid value for --last '<argument>': it is not a positive whole number. Run 'bucu --help' for usage.` and exits `2`; the argument is shown as given, with each control character written as an escape. `list --last` with ID arguments prints `bucu: --last cannot be used with IDs. Run 'bucu --help' for usage.` and exits `2`.
- A single ID that is not stored prints `bucu: no bookmark has ID <n>. Run 'bucu list' to see IDs.` A range with no stored ID prints `bucu: no bookmark has an ID in <a>-<b>. Run 'bucu list' to see IDs.` With no ID and no bookmark stored, `bucu` prints `bucu: there are no bookmarks.` Each goes to standard error, changes nothing, and exits `5`.
- A change to every bookmark in a run that is not interactive, without `--yes`, prints `bucu: changing all bookmarks needs confirmation; add --yes to confirm. Run 'bucu --help' for usage.` and exits `2`.
- A `delete` in a run that is not interactive, without `--yes`, prints `bucu: deleting bookmarks needs confirmation; add --yes to confirm. Run 'bucu --help' for usage.` and exits `2`. A `reorder` that needs to ask prints `bucu: renumbering bookmarks needs confirmation; add --yes to confirm. Run 'bucu --help' for usage.` and exits `2`.
- `swap` with other than two single IDs prints `bucu: swap takes two IDs. Run 'bucu --help' for usage.`; with two equal IDs, `bucu: swap needs two different IDs. Run 'bucu --help' for usage.`; both exit `2`.
- `reorder` with an ID prints `bucu: reorder takes no ID. Run 'bucu --help' for usage.` and exits `2`.
- The confirmation prompts (`Update all <n> bookmarks? [y/N] `, `Delete <n> bookmarks? [y/N] `, `Delete all <n> bookmarks? [y/N] `, `Renumber <m> bookmarks? [y/N] `) and `Cancelled.` go to standard output, not standard error. So do `Deleted <n> bookmarks.`, `Renumbered <m> bookmarks.`, and `IDs are already 1-<n>.`.
- When writing standard output fails, `bucu` prints `bucu: cannot write to standard output` to standard error and exits `1`.

## 7. Import and export formats (added by #17, #18)

HTML (Netscape bookmark file) and Markdown. Shapes and duplicate handling: settled by #17 and #18.

## 8. Network fetch (added by #16)

Title and description fetch on `add`. Default, timeouts, and limits: settled by #16.
