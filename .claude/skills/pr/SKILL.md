---
name: pr
description: SDD step 4. Run the full checks, tick the tasks and acceptance criteria in the issue body, and open the pull request.
disable-model-invocation: true
argument-hint: [issue-number]
arguments: [issue]
model: sonnet
---

Open the pull request for issue #$issue.

1. Run the full build and every test (`CLAUDE.md` Commands), and the `asan-ubsan` preset when C++ changed (constitution §3). This is the first full run since the baseline: fix a failure in a new commit, as a task of its own when it changes behaviour.
2. Confirm every approved Documentation update and contract record is applied (constitution §3). When one is missing, stop and report it.
3. Tick the Tasks part: check each task that has a commit whose subject starts with its ID. A task with no commit is a gap: stop and ask.
4. Complete the evidence of each acceptance criterion (Spec part; for Tier 0, each Scope bullet). Each criterion declared a kind when it was planned, as a trailing `Evidence: <kind>`; append only where the evidence is to that same field. Never write a kind here. `tools/sdd_check.py` checks these forms.

   | Kind | Completed form | Box | Meaning |
   | --- | --- | --- | --- |
   | `test` | `Evidence: test: <test file>: <test case name>` | checked | The test is in the diff and proves the criterion |
   | `property` | `Evidence: property: <test file>: <property name>` | checked | A property test in the diff proves it; it counts as the test of constitution principle 4 |
   | `fuzz` | `Evidence: fuzz: <target file>: <what ran>` | checked | The fuzz target ran clean; its `[V]` task sits beside a test of the same behaviour |
   | `proof` | `Evidence: proof: <harness file>: <property proved>` | checked | The proof harness ran and passed; its `[V]` task sits beside a test of the same behaviour |
   | `check` | `Evidence: check: <task IDs>` | checked | The `Check:` of each `[V]` task ran and passed |
   | `manual` | `Evidence: manual: check <n> (owner)` | unchecked until the owner checks it | The owner checks it before the merge |
   | `after merge` | `Evidence: after merge (owner)` | unchecked until the owner checks it | Proof only after the merge, for example a workflow on `master` |

   Evidence of a different kind than the line names is a deviation: stop (`/implement` step 9); plan mode rewrites the line. A different location, with the same kind, is not. A criterion proven by two kinds is split into two criteria at plan time, never at `/pr`. A criterion with no evidence is a gap: stop and ask. Write the Spec and Tasks parts into the issue body in one edit ([issue-body.md](../plan/issue-body.md)).
5. Run `python3 tools/sdd_check.py issue $issue`; when it fails, stop and report it.
6. Push. Open the PR from `.github/pull_request_template.md`, with `Closes #$issue`: `gh pr create --repo grgomez/buCu --base master --body-file <file>`. Always pass `--repo` and `--base`: the repository began as a fork, and a PR must never go upstream. Do not request reviewers. Never auto-merge.
7. Run `tools/board.sh $issue "In review"`; when it fails, ask the owner to move the item.
8. Report the PR link and any check that failed.
