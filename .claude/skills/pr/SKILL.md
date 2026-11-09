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
4. Add evidence to each acceptance criterion (Spec part; for Tier 0, each Scope bullet), appended to the end of its line:
   - A test: `Evidence: <test file>: <test case name>`, checked. The test is in the diff and proves the criterion.
   - A verification task: `Evidence: <task ID> Check`, checked. The `Check:` ran and passed.
   - A manual check: unchecked, `Evidence: manual, check <n> (owner)`. The owner checks it before the merge.
   - Proof only after the merge (for example a workflow on `master`): unchecked, `Evidence: after merge (owner)`.

   A criterion with none of these is a gap: stop and ask. Write the Spec and Tasks parts into the issue body in one edit ([issue-body.md](../plan/issue-body.md)).
5. Run `python3 tools/sdd_check.py issue $issue`; when it fails, stop and report it.
6. Push. Open the PR from `.github/pull_request_template.md`, with `Closes #$issue`: `gh pr create --repo grgomez/buCu --base master --body-file <file>`. Always pass `--repo` and `--base`: the repository began as a fork, and a PR must never go upstream. Do not request reviewers. Never auto-merge.
7. Run `tools/board.sh $issue "In review"`; when it fails, ask the owner to move the item.
8. Report the PR link and any check that failed.
