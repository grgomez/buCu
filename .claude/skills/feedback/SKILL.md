---
name: feedback
description: SDD step 5. Handle the review comments and failed checks on the pull request of an issue.
disable-model-invocation: true
argument-hint: [issue-number]
arguments: [issue]
model: sonnet
---

Handle the review comments on the pull request for issue #$issue.

1. Read `docs/constitution.md`, the living-document sections the parts link, and the parts in the issue body: `gh issue view $issue --json body -q .body`.
2. Run `git pull --ff-only` so the branch includes any suggestion batch the owner committed in GitHub. Find the pull request and read every comment: the conversation, the reviews, the inline comments, and the logs of failed checks.
3. For each comment or failure, list it and the change you propose, and ask the owner which to apply. When the comments came from a `/review` table, post one PR comment, "Review findings", once the owner decides: the table with a Decision column.
4. Apply the approved items of the round that change no behaviour in one commit. Start each commit subject with the review finding ID, `R<n> <summary>` (the commit-msg hook accepts it). Give each item that changes behaviour its own commit, test first, as one red, green, refactor cycle (constitution principle 4); a bug fix gets a regression test. Before each commit, run the targeted tests of the changed behaviour. Before the push, run the full build and tests when the round changed code.
5. Reply to each comment with what changed, or why nothing changed.
6. If a change must differ from the approved parts, stop: it is a deviation (`/implement` step 9).
7. After each round that adds, renames, or removes a task, changes a criterion, or changes the evidence of a criterion, update the Spec and Tasks parts in the issue body to match (`/pr` steps 3 and 4).
