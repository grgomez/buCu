---
name: review
description: SDD step 5. Review the branch changes of an issue in a separate context and report PR comments.
disable-model-invocation: true
argument-hint: [issue-number]
arguments: [issue]
context: fork
disallowed-tools: Edit Write NotebookEdit
model: opus
---

Review the changes on the current branch for issue #$issue. Do not edit any file.

1. Read `CLAUDE.md`, `docs/constitution.md`, architecture §4, §7, §8, and §9, and the living-document sections the parts link. Read the Spec, Plan, and Tasks parts in the issue body: `gh issue view $issue --json body -q .body`. A Tier 0 issue has no Spec part; its body is the scope.
2. Read the diff: `git --no-pager diff origin/master...HEAD`, and the commits: `git log origin/master..HEAD`. Also check: every acceptance criterion traces to a task, every changed code file to a task commit, and every deviation from the approved parts went through plan mode (an unapproved deviation is Critical).
3. Apply these checks:
   - Clarity: readable code, no needless complexity, no hidden side effects.
   - Correctness and safety: behaviour matches the Spec part and `docs/product.md`; module boundaries (architecture §4) hold; errors are handled (architecture §7); ownership and lifetimes are sound (RAII, no dangling `std::string_view`, no undefined behaviour); output and exit codes match product §5 and §6 in non-interactive runs; each schema, export-format, or public-header change has a contract record that matches the code and its tests (architecture §5).
   - Portability: no operating-system code outside `src/platform/`; UTF-8 and path handling hold on Windows; nothing relies on GCC-only or Clang-only behaviour.
   - Licensing: no upstream notice removed; no help text, message, or fixture copied from Buku without need and attribution (constitution principle 11).
   - Consistency: naming, patterns, and structure match the codebase (architecture §8).
   - Tests: each acceptance criterion has a test, a manual check, or the `Check:` of a `[V]` task (constitution principle 4); tests follow architecture §9; each bug fix has a regression test.
   - Criteria evidence (`/pr` step 4): the named evidence is in the diff or the PR, and it proves the criterion. Evidence that does not prove its criterion is Critical.
4. Report a table with these columns: ID (R1, R2), Severity (Critical, Warning, Cleanup), Category (spec gap, plan gap, test gap, code defect, portability, convention), File, Line, PR comment. Each PR comment states the problem and a concrete fix, ready to paste.
