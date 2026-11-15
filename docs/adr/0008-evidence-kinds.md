# ADR 0008: Evidence kinds

- Status: Accepted
- Date: 2026-11-14
- Issue: #64
- Amends: ADR 0003 (the acceptance-criterion evidence rule, and principle 4 in the constitution)

## Context

ADR 0003 made the issue body the home of the Spec, Plan, and Tasks, and `/pr` step 4 adds evidence to each acceptance criterion at the end of the work. Only four formats exist: a test, a verification task, a manual check, and proof after the merge. Nothing is declared when the criterion is planned, so `/spec`, `/plan`, and Analyze cannot check that each criterion can be proven. Later parts of #62 add property tests, fuzz targets, and proofs, and need a named place in the workflow.

## Considered options

- **Declare a kind at plan time and extend it in `/pr` (chosen):** one field per criterion, so the kind is fixed by construction: `/pr` never writes a kind.
- **Choose the kind only in `/pr`:** nothing can check the plan, and a criterion with no possible evidence is found last.
- **A separate evidence table in the Plan part:** a second place to keep in step with the criteria.

For principle 4:

- **A property test is a test; a fuzz target or proof harness is a `[V]` task beside a test of the same behaviour (chosen):** a property runs in `ctest` and can fail first; a fuzz run or proof has no reliable red step.
- **Let a fuzz target or proof stand alone:** a criterion could be proven only by an input search or a proof that never ran red.

## Decision

We will use seven evidence kinds: `test`, `property`, `fuzz`, `proof`, `check`, `manual`, and `after merge`. Each criterion (a Spec part's `USn-ACm`, or a Tier 0 Scope bullet) ends with `Evidence: <kind>` when it is planned. `/pr` step 4 appends only where the evidence is, and owns the kinds and the completed formats. Delivering a different kind than the one on the line is a deviation: the body is rewritten through plan mode (`/implement` step 9) before `/pr`. `/review` checks that the evidence matches the kind on its line and proves the criterion. A criterion proven by two kinds is split into two criteria at plan time.

A property test counts as the test of constitution principle 4. A fuzz target or proof harness is a `[V]` task that sits beside a test of the same behaviour.

`tools/sdd_check.py issue` fails on a criterion with no valid kind. `tools/sdd_check.py pr` fails on a ticked `test`, `property`, `fuzz`, `proof`, or `check` line without its location, and on an unticked line that is not a completed `manual` or `after merge` form. A ticked completed `manual` or `after merge` line passes, because the owner checked it.

## Consequences

- A criterion that cannot be proven is found at plan time, not in `/pr`.
- A criterion needing two kinds takes two criteria, which counts toward the limit of 12.
- A new kind is a change to `/pr` step 4 and `sdd_check.py` only.
- An issue body in the old format fails the check. No open issue with parts uses it.
- A criterion proven only by fuzzing or a proof is flagged by Analyze, because it has no test task.
