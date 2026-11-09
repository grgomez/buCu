---
name: plan
description: SDD step 2. In plan mode, write the Plan and Tasks parts of an issue (and its Spec part for Tier 1); on acceptance, record them in the issue body and start Implement.
disable-model-invocation: true
argument-hint: [issue-number]
arguments: [issue]
model: opus
effort: high
---

Plan issue #$issue. Do not write code before the owner accepts the plan.

1. Read the issue (`gh issue view $issue`) and any parts already in its body, `docs/constitution.md`, the headings of `docs/product.md` and `docs/architecture.md` (`grep -n '^#'`), product §2 (Glossary), only the sections the issue touches, architecture §4 (modules), `docs/adr/README.md` and the ADRs the issue touches, and the code and tests it touches.
2. Run `tools/start.sh $issue` (`CLAUDE.md` step 0) when the current branch is not the issue's branch.
3. Propose the tier (`CLAUDE.md` Scope rule) with a one-line reason; the owner confirms it by accepting the plan. For Tier 2, the issue must have the `spec-approved` label and a Spec part; otherwise stop and name `/spec $issue`.
4. List each third-party unknown the plan depends on, and run Trial (below) for each one before you enter plan mode. Trials build code, so they cannot run in plan mode.
5. Enter plan mode (EnterPlanMode).
6. Tier 1: draft the Spec part from [the spec template](../spec/template.md), and clarify it with the owner as `/spec` steps 5 and 6 describe. Tier 0: no Spec part; the issue body is the scope, and its "Open product questions" are asked here.
7. Draft the Plan part from [template.md](template.md). Write an optional section only when its condition holds. Write an ADR only when the change makes a major decision. List each contract record (`docs/architecture.md` §5) and every living-document change under "Documentation updates".
8. Draft the Tasks part from [tasks-template.md](tasks-template.md) and count every task line. Above the size limit, run Split (below) and stop.
9. Tier 2 (Tier 1 when the owner asks): run Analyze (below) and record its table, with the owner's decisions, in the Plan part's "Analyze" section. Apply each "fix" to the plan text before you exit.
10. Write the plan-mode text: the lines `Issue: #$issue`, `Stage: Plan`, and `Tier: <n>`, then the Spec part (Tier 1 only), the Plan part, and the Tasks part. No `[NEEDS CLARIFICATION]` marker may remain. Exit plan mode (ExitPlanMode).
11. When the owner rejects it, revise from their feedback and exit plan mode again.
12. When the owner accepts: confirm that the issue has the `approved` label and the tier label. When either is missing, the approval hook failed: stop and tell the owner. Never add `approved` yourself.
13. Write the accepted parts into the issue body ([issue-body.md](issue-body.md)). Create each "new issue" decision from Analyze with `gh issue create`, its body holding the finding and "Found by Analyze of #$issue".
14. Follow `.claude/skills/implement/SKILL.md` in this session.

## Trial

A third-party unknown is a fact the plan depends on about a third-party tool: its version, a version pairing, support on a compiler or platform (for example an MSVC C++20 feature, or how libcurl reaches the Windows build), or its behaviour in this build. A fact the current build on `master` already confirms is not one. With no unknown, write "No trial needed" in "Trials" and create no branch.

- Start: `git fetch origin`, then `git worktree add --no-track -b trial/issue-<n>-<topic> <dir> origin/master`, with `<dir>` in a new temporary directory outside the repository (`mktemp -d`). Never push the branch. If an earlier trial was cut off, first run `git worktree prune` and `git branch -D` on its `trial/` branch.
- Run on Linux, or in a CI run on a throwaway branch only when the owner agrees; record the operating system.
- Timebox: 3 attempts per unknown. An attempt is one change followed by one build or run that observes the result.
- Record the result in the Plan part's "Trials". Never copy trial code into the issue branch. Implement reuses only the recorded facts, and writes every line under constitution principle 4. A new dependency still needs a plan decision (constitution §4).
- Stop and ask the owner when 3 attempts give no answer (mark the unknown `[NEEDS CLARIFICATION]`), or when a result contradicts the Spec part or `docs/architecture.md`.
- End: `git worktree remove --force <dir>`, `git branch -D trial/issue-<n>-<topic>`, and delete the temporary directory. Check that `git worktree list` and `git branch --list 'trial/*'` show neither.

## Analyze

Run one full review in a read-only subagent (`Plan` type, model `opus`), given the issue number and the draft parts. It reads `docs/constitution.md`, the living documents the parts link, and `docs/adr/`, and reports only findings that would change the code, a test, or a documented rule; never wording or style. It returns one table, Critical findings first, with the columns ID (A1, A2), Severity, Check, Section, Finding, Suggested fix.

| # | Check | Severity |
| --- | --- | --- |
| 1 | Coverage: from the `Covers:` lines, requirements and acceptance criteria with no task, and tasks that cover nothing without a reason. | High |
| 2 | Consistency: terms, numbers, or behaviour that differ between the parts, or from the living documents. | High |
| 3 | Ambiguity: vague or untestable content. A leftover `[NEEDS CLARIFICATION]` marker is Critical. | Medium |
| 4 | Constitution: a principle, gate, or DO NOT item that is violated. | Critical |
| 5 | Architecture: a module responsibility change with no ADR, or a change to the schema, an export format, or a public header with no contract record (`docs/architecture.md` §5). | High |
| 6 | Trial: a decision that rests on a third-party unknown with no trial result. | High |

Show the table and ask the owner to decide every finding in one batch: fix, drop, or new issue. Only an open Critical finding blocks the exit from plan mode.

## Split

1. Propose the parts to the owner: the title and scope of each, each within the size limit. Write nothing until the owner decides. The owner's acceptance covers the issue creation. When the owner declines, the owner cuts the scope, and the calling step runs again from its size check.
2. Leave plan mode when you are in it, then create each part with `gh issue create --title <title> --body-file <file>`, the body holding its scope and the "Open product questions" that apply. Link each part to the parent: `gh api repos/{owner}/{repo}/issues/<sub> --jq .id`, then `gh api -X POST repos/{owner}/{repo}/issues/<parent>/sub_issues -F sub_issue_id=<id>`. When the call fails, ask the owner to link them by hand.
3. Delete the parent's branch when it has no commits of its own: `git switch master`, `git branch -D <branch>`, and `git push origin --delete <branch>` when it is on `origin`.
4. Stop. The owner adds each part to the board and starts each part at step 0.
