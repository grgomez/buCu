# Tasks

Rules:

- Every task names its test file and the code file it changes. A `[V]` task names the file it changes and its check instead.
- Every task has a `Covers:` line naming the requirements and acceptance criteria it proves, for example `Covers: FR-001, US1-AC2`. A task that proves no requirement (setup, documentation, checks) writes `Covers: none (<reason>)`. A Tier 0 issue has no Spec part; its tasks cover the issue's Scope bullets, for example `Covers: scope 2`.
- Each task that changes behaviour is one red, green, refactor cycle for one behaviour, and one commit (constitution principle 4).
- `[V]` marks a verification task: it changes nothing a test can observe, so it has no red step. It has a `No test:` line (why no test can observe it) and a `Check:` line (a command, or a read of the changed file against named FR, AC, or scope IDs, that Implement runs before the commit). A task qualifies when it changes nothing a test can observe, for example CMake files, CI, tool configuration, vendored code, or documentation.
- A task that mixes a behaviour change with configuration stays a red, green, refactor task, unless you split it into two tasks.
- `[P]` marks a task that can run in parallel with the other `[P]` tasks in the same group (different files, no shared dependency).
- Order: setup first, then each user story in priority order, then Finish. No task pushes or opens the PR; that is step 4 (`/pr`).

## Setup

- [ ] T001 [V] <shared setup, for example a CMake change> in `<path>`
  No test: <why no test can observe it>.
  Check: <the command or read that Implement runs before the commit>.
  Covers: none (setup)

## US1: <short title>

- [ ] T002 [P] <behaviour>: test in `tests/<module>/<name>_test.cpp`, code in `src/<module>/<name>.cpp`
  Covers: FR-001, US1-AC1

## Finish

- [ ] T003 [V] Apply the approved Documentation updates (constitution §3)
  No test: the updates are documents.
  Check: `git diff` shows each approved update and no other document.
  Covers: none (documentation)
- [ ] T004 [V] Run every check in `CLAUDE.md` Commands, the sanitizer preset included when C++ changed
  No test: a check run changes no behaviour.
  Check: every command passes.
  Covers: none (checks)
