---
name: spec
description: SDD step 1, Tier 2 only. Write the Spec part of an issue in plan mode; on acceptance, record it in the issue body.
disable-model-invocation: true
argument-hint: [issue-number]
arguments: [issue]
model: opus
---

Write the spec for issue #$issue. Do not write code.

1. Read the issue (`gh issue view $issue`), `docs/constitution.md`, the headings of `docs/product.md` and `docs/architecture.md` (`grep -n '^#'`), product §2 (Glossary), `docs/adr/README.md`, and only the sections the issue touches.
2. Run `tools/start.sh $issue` (`CLAUDE.md` step 0) when the current branch is not the issue's branch. When the script cannot move the board item, ask the owner to move it.
3. Propose the tier (`CLAUDE.md` Scope rule) with a one-line reason. When it is not Tier 2, stop and tell the owner to run `/plan $issue` instead.
4. Enter plan mode (EnterPlanMode).
5. Draft the Spec part from [template.md](template.md). Turn each "Open product question" in the issue into a question, and mark every other gap or conflict with the living documents as `[NEEDS CLARIFICATION]`. Ask the owner about them as `CLAUDE.md` "Working with the user" describes. After each answer, update the text and add the Q and A under "Clarifications". When an answer changes a product rule, add it under "Product rules", Changes.
6. Check the size limit (`CLAUDE.md` Scope rule). Above it, run Split (`/plan` Split) and stop.
7. Write the plan-mode text: the lines `Issue: #$issue`, `Stage: Spec`, and `Tier: 2`, then the Spec part. No `[NEEDS CLARIFICATION]` marker may remain. Exit plan mode (ExitPlanMode).
8. When the owner rejects it, revise from their feedback and exit plan mode again.
9. When the owner accepts: confirm that the issue has the `spec-approved` label (`gh issue view $issue --json labels -q '.labels[].name'`). When it is missing, the approval hook failed: stop and tell the owner. Never add the label yourself.
10. Write the Spec part into the issue body ([issue-body.md](../plan/issue-body.md)). Stop, and name `/plan $issue` as the next step.
