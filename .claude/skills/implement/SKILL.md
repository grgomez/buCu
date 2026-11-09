---
name: implement
description: SDD step 3. Implement the tasks of an approved issue, red-green-refactor for each behaviour change. Runs right after /plan; call it directly to resume.
disable-model-invocation: true
argument-hint: [issue-number]
arguments: [issue]
model: sonnet
---

Implement issue #$issue.

1. Read the Spec, Plan, and Tasks parts in the issue body (`gh issue view $issue --json body -q .body`), `docs/constitution.md`, and only the living-document sections the parts link, plus architecture §8 and §9 (coding and testing standards).
2. Confirm the issue has the `approved` label (`gh issue view $issue --json labels -q '.labels[].name'`) and that you are on the issue's branch. When the label is missing, stop and name `/plan $issue`; never add it yourself.
3. Find the progress: `git log --format=%s origin/master..HEAD` lists the task IDs already committed. Start at the first task with no commit. Run the full build and tests (`CLAUDE.md` Commands) once to get a baseline.
4. Work through the Tasks part in order, one task at a time.
5. Behaviour task: one cycle, one commit (constitution principle 4).
   1. Red: write the test and confirm it fails for the right reason. Build, then run only the task's tests (`ctest --preset debug -R '<regex>'`), here and in the next two steps.
   2. Green: write the smallest code that makes it pass.
   3. Refactor: clean up only the code this task wrote or changed, in the files the task names, without changing behaviour. Keep the task's targeted tests green.
   4. Run the task's targeted tests. Do not move on while anything fails. The pre-commit hook checks formatting (`scripts/format.sh` fixes it).
   5. Make one commit with only that task's test and code. The subject starts with the task ID, with no type: `T004 <summary>` (constitution principle 10). Below the subject, write one line for each step of the cycle, even when a step changed nothing:
      ```text
      Red: <the test, and how it failed>
      Green: <the code added, or "none, already passing">
      Refactor: <what changed, or "reviewed <files>, no change">
      ```
6. `[V]` task: no cycle. Make the change, run its `Check:` and the task's targeted tests, if any. Make one commit with only that task's change: `T004 <summary>`, and below it:
   ```text
   Change: <what changed>
   Check: <the check that ran, and its result>
   ```
   For both kinds of task: never combine tasks in one commit, and never commit while anything fails. Do not edit the issue body per task; `/pr` ticks the tasks.
7. Do not run the full build per task; `/pr` runs it (constitution §3). On a failure, read only the failed target's or test's error, not the whole log.
8. Change only the files the task names. If something else must change, or a wider refactor looks worthwhile, stop and ask.
9. Deviation: when the code must differ from the Spec or Plan part, stop. Enter plan mode with the plan-mode text of `/plan` step 10, the changed parts, and a "Deviation" note at the top saying what changes and why. When the owner accepts, write the changed parts into the issue body ([issue-body.md](../plan/issue-body.md)) and continue (constitution principle 3).
10. Apply only the document edits approved under "Documentation updates". Write each contract record the Plan part lists, with `Status: Proposed`; set it to `Accepted` when every task is committed.
11. When every task is committed, push and follow `.claude/skills/pr/SKILL.md`.
