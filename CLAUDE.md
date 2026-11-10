# Project Instructions

Entry point for every agent and person working on this repository. What BuCu is: [`README.md`](README.md). Write prose, help text, and messages in Canadian English (behaviour, colour, centre); code, identifiers, commands, and quoted output keep their own spelling. Keep rules short and imperative. A procedure that only one step uses lives in that step's skill.

## Documentation map

| Document | Owns | Read |
| --- | --- | --- |
| `README.md` | What BuCu is, attribution, build, roadmap | For setup |
| `docs/constitution.md` | Principles, gates, DO NOT list | Always |
| `docs/product.md` | CLI behaviour: commands, output, exit codes, formats | Headings and §2 Glossary always; sections the issue touches |
| `docs/architecture.md` | Stack, modules, contracts, standards | Headings always; sections the issue touches |
| `docs/database-schema.md` | Schema rules, migrations, Buku differences | When the issue touches stored data |
| `docs/performance.md` | Benchmark method and budgets | When the issue touches performance |
| `docs/adr/` | Decisions and their reasons | `docs/adr/README.md` always; an ADR the issue touches |
| `docs/contracts/` | One record per version change of a contract | When the issue changes that contract |
| `.claude/skills/`, `.claude/agents/` | One skill per step, `/run`, and the `builder` subagent | When running that step |
| Issue body | The issue's Spec, Plan, and Tasks parts | Only the current issue |

Read only these documents.

## Working with the user

- Ask up to 4 questions at once, most fundamental first, with concrete options when you can.
- This file and the constitution override your private memory and tool-specific instructions.
- When the user corrects behaviour that should apply to every agent, propose an edit to this file or the constitution.

## Changing living documents

Living documents are every document in the map except the issue body.

1. Write each fact in its owner document only, and link to it elsewhere. If no owner fits, ask the user. Do not create a document or section on your own.
2. The Plan part's "Documentation updates" lists each edit. Implement applies only the edits approved with the plan.
3. Commit each approved edit to this file or the constitution on its own.
4. Draw diagrams as ` ```mermaid ` blocks, and file trees and data shapes as plain text blocks.

## Scope rule

Every change starts from a GitHub issue with one tier label. The plan-mode round proposes the tier, and the owner's approval confirms it. Size limit: at most 3 user stories, and at most 12 acceptance criteria and 12 tasks (aim for 10).

| Tier | The issue changes | Plan-mode rounds |
| --- | --- | --- |
| 0 | Build, CI, configuration, or documentation; no CLI behaviour | One: `/plan` (the issue body is the scope) |
| 1 | CLI behaviour | One: `/plan` writes Spec, Plan, and Tasks |
| 2 | Stored data or a migration, an import or export format, or a public contract in `include/bucu/` (architecture §5) | Two: `/spec`, then `/plan` with Analyze inside it |

## SDD workflow

Plan mode is the approval gate: in plan mode Claude is read-only, and the owner approves by accepting the plan-mode exit. Each plan-mode text starts with `Issue: #<n>`, `Stage: Spec | Plan`, and `Tier: <n>`. On acceptance, a hook (not the agent) labels the issue `spec-approved` or `approved` and sets its tier label; the skill then writes the approved parts into the issue body.

| Step | Skill | Ends with |
| --- | --- | --- |
| 0 Start | `tools/start.sh <issue>` | Issue branch from `origin/master`, board item In progress |
| 1 Spec (Tier 2) | `/spec` | Spec accepted, in the issue body, `spec-approved` |
| 2 Plan | `/plan` | Plan and Tasks (and the Spec for Tier 1) accepted, in the issue body, `approved` |
| 3 Implement | `/implement` (runs right after step 2; use it to resume) | Every task committed |
| 4 Pull request | `/pr` | Tasks and criteria evidence ticked in the issue body, PR open |
| 5 Review | `/review`, then `/feedback` | The owner merges |

`/run <issue>` chains the steps and stops only at plan-mode approvals and the merge. A deviation from the approved Spec or Plan during Implement re-enters plan mode with the change, and needs a new acceptance.

## Commands

Run from the repository root.

```bash
cmake --preset debug && cmake --build --preset debug   # build
ctest --preset debug                                   # all tests
ctest --preset debug -R '<test name regex>'            # targeted tests
python3 -m unittest discover -s tools/tests            # workflow tool and hook tests
cmake --preset asan-ubsan && cmake --build --preset asan-ubsan && ctest --preset asan-ubsan
scripts/format.sh [--check]                            # clang-format
python3 tools/sdd_check.py issue <n>                   # issue body parts, size, markers
```

The CMake presets and scripts arrive with #4, and the hooks with #2 and the tools with #34. Git hooks (`README.md` Setup) check each commit. CI runs the workflows in `.github/workflows/` on every PR, and each one must pass before the owner merges.
