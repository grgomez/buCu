# Constitution

Stable principles and gates for every change, by any agent or person. Which document owns each topic is in the `CLAUDE.md` Documentation map.

## 1. Principles

1. **KISS.** If an existing pattern can meet the requirement, do not add a new subsystem.
2. **Anti-abstraction.** Do not add abstractions that current requirements and tests do not need. The platform and network interfaces (architecture §4) are required by portability, not speculation.
3. **Specs rule.** The approved Spec part and `docs/product.md` define behaviour. If code and spec disagree, fix the code, or change the spec through a new plan-mode acceptance with a reason.
4. **Red, green, refactor.** For each task, write or update the failing test first, then the smallest code that makes it pass, then refactor that code with the tests green. A verification task (`[V]`, rules in `.claude/skills/plan/tasks-template.md`) has no red step and names the check that proves it.
5. **No assumptions.** When anything in a spec, plan, task, or acceptance criterion is ambiguous, mark it `[NEEDS CLARIFICATION: <question>]` and ask the user in the plan-mode round. Never guess, and never leave plan mode with an open marker.
6. **Single source of truth.** Every fact has one owner document. Other documents link to it.
7. **Human in control.** The user approves each Spec and Plan by accepting the plan-mode exit, every living-document change, and the merge. Agents propose; the user decides.
8. **Network only where asked.** Only `src/net/` opens network connections. Every network operation is optional, bounded by a timeout, and never blocks a core operation; `--offline` disables it (product §2).
9. **Layered modules.** Keep the module boundaries and dependency direction in architecture §3 and §4. Only `src/platform/` contains operating-system-specific code.
10. **Smallest change.** Make the smallest change that solves the task: one logical change per commit, conservative and testable.
11. **Honour the upstream.** BuCu is a port of Buku by Arun Prakash Jana, under GPL-3.0-or-later. Keep every upstream copyright and licence notice. Write original help text, messages, documentation, and test fixtures; copy from Buku only when necessary, with attribution.
12. **Portable by default.** Code builds and passes its tests on every platform in the CI matrix (architecture §2). UTF-8 everywhere; paths go through `std::filesystem::path` built from UTF-8.

## 2. Where the details live

CLI behaviour: `docs/product.md`. Stack, modules, contracts, and coding, testing, and portability standards: `docs/architecture.md`. Stored data: `docs/database-schema.md`. Performance method and budgets: `docs/performance.md`.

## 3. Gates

A change is done only when all of these hold:

- Each commit: the targeted tests of the task, when it has any, pass; clang-format passes for the staged C++ files (pre-commit hook).
- Before the PR (`/pr`): the full build and every test pass with the `debug` preset, and with the `asan-ubsan` preset when C++ changed (`CLAUDE.md` Commands).
- CI on the PR: the build and test matrix passes with warnings as errors; `sdd-gates.yml` checks each gate marked (sdd-gates).
- Principles 3 and 4 hold: code agrees with the spec, and every task in the Tasks part of the issue body is checked (sdd-gates) and follows principle 4.
- Each acceptance criterion is checked with its evidence, or marked manual or after merge (sdd-gates).
- Implement starts only when the issue has the `approved` label, set by the plan-mode approval hook (Claude Code hook).
- Every living-document change was approved with the plan and is listed in the PR's "Living documents" section (sdd-gates).
- Affected living documents are updated in the same PR.
- No rule or decision lives only in the issue body: each product rule is in `docs/product.md`, each system decision in `docs/architecture.md` or a new ADR, and each contract change in a record in `docs/contracts/`.
- Every bug fix has a regression test.
- The PR says `Closes #<number>` (sdd-gates).

## 4. DO NOT

- DO NOT add a dependency that architecture §2 does not list. A new dependency needs a plan decision with its reason, its licence, and an update to architecture §2.
- DO NOT add network access outside `src/net/`, or analytics, telemetry, or crash reporting anywhere.
- DO NOT break a module's "MUST NOT" rule in architecture §4, or move responsibilities between modules, without an ADR.
- DO NOT remove or obscure an upstream copyright or licence notice.
- DO NOT change files outside the task scope.
- DO NOT copy a rule, command, table, or contract into a second document. Link to its owner.
- DO NOT hard-code test counts in any document. CI reports them.

## 5. Governance

- Change this constitution only through a PR that explains the reason.
- Record a change to a principle, a gate, or the `CLAUDE.md` Documentation map in a new ADR.
- Keep this file under 150 lines.

## 6. Normative language

- MUST, MUST NOT, SHOULD, SHOULD NOT, and MAY in the living documents and the issue-body parts have the meaning in BCP 14 ([RFC 2119](https://www.rfc-editor.org/rfc/rfc2119), [RFC 8174](https://www.rfc-editor.org/rfc/rfc8174)), and only when written in UPPERCASE. In lowercase, they are ordinary English.
- Each MUST in a merged feature is enforced by a test that cites its section, or by a gate in §3.
