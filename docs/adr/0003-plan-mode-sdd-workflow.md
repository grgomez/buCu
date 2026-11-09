# ADR 0003: SDD with plan mode as the approval gate

- Status: Accepted
- Date: 2026-11-09
- Issue: #1

## Context

BuCu adopts the spec-driven development (SDD) workflow of the owner's fitr-android project: living documents, tiers, red-green-refactor tasks, and one owner approval before code (fitr ADRs 0010, 0015, 0016). In fitr, the spec, plan, and tasks live in a `specs/<issue>-<name>.md` file on the branch, the owner approves by adding the `approved` label, and `/pr` copies the file into the issue body and deletes it. BuCu is developed with Claude Code only, whose plan mode is read-only until the user accepts the plan.

## Considered options

- **Copy fitr's workflow unchanged:** proven, but keeps the spec file, its sync into the issue, its deletion, and a separate label click for approval.
- **Plan mode as the gate, issue body as the record (chosen):** plan mode enforces "no code before approval"; acceptance is the approval; the approved text goes straight into the issue body.
- **No formal workflow:** loses the spec approval before code and the durable documents.

## Decision

We will run each SDD round in plan mode. Tier 0 and Tier 1 have one round (`/plan`); Tier 2 has two (`/spec`, then `/plan` with Analyze inside it). Each plan-mode text starts with `Issue:`, `Stage:`, and `Tier:` lines. A Claude Code PostToolUse hook on `ExitPlanMode`, which fires only on acceptance, adds `spec-approved` or `approved` and the tier label, so the hook records the owner's approval and agents still cannot add those labels. The skill then writes the accepted parts into the issue body between `SPEC`, `PLAN`, and `TASKS` markers. Tasks are ticked once, at `/pr`; commit subjects carry task IDs, so a resumed session reads progress from `git log`. There is no `specs/` directory.

The tiers, size limit, red-green-refactor and verification tasks, Trials, contract records, and the review steps stay as in fitr.

## Consequences

- No spec-file sync or deletion commits; the issue body is the only record, and GitHub keeps its edit history instead of git.
- Tier 2 has two approvals, which fitr's ADR 0010 avoided; the extra stop is limited to stored data, formats, and public contracts.
- The workflow depends on Claude Code plan mode and its hook payload; #2 confirms the hook behaviour with a trial, and the fallback is the owner adding the label by hand.
