# Writing parts into the issue body

Used by `/spec`, `/plan`, `/pr`, and `/feedback`. The issue body is the record of each approved part (`CLAUDE.md` SDD workflow).

Layout: the owner's original text first, then one `---` line, then the parts in this order, each between its markers. A marker counts only when it is alone on its line.

```text
<original issue text>

---
<!-- SPEC:START -->
# Spec
...
<!-- SPEC:END -->
<!-- PLAN:START -->
# Plan
...
<!-- PLAN:END -->
<!-- TASKS:START -->
# Tasks
...
<!-- TASKS:END -->
```

1. Read the body: `gh issue view <n> --json body -q .body > <tmp>/body.md`, with `<tmp>` a new temporary directory outside the repository (`mktemp -d`).
2. For each part being written, replace the text between its markers with the approved part, unchanged. When the markers are absent, append them in the order above; add the `---` line before the first part.
3. Write the new body with `gh issue edit <n> --body-file <tmp>/body.md`, so backticks, quotes, and `$` stay intact. Never pass the body on the command line.
4. Read the body back and confirm each part is present. When GitHub rejects the body as too long, stop and ask the owner.

The plan-mode text holds the header lines (`Issue:`, `Stage:`, `Tier:`) above the parts; do not copy them into the body.
