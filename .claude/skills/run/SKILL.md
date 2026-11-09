---
name: run
description: Run the SDD workflow for an issue with one command. Plan-mode rounds in this session, build in a subagent, then review. Stops only at plan-mode approvals and the merge.
disable-model-invocation: true
argument-hint: [issue-number]
arguments: [issue]
model: opus
---

Run issue #$issue through the `CLAUDE.md` SDD workflow. For each step, read that step's `SKILL.md` and follow it. Do not invoke the step skills: they are user-only. Every rule in those files still applies, except where this file says otherwise.

## Spec and plan (this session)

1. Propose the tier (`CLAUDE.md` Scope rule). For Tier 2 without the `spec-approved` label, follow `.claude/skills/spec/SKILL.md` through its plan-mode acceptance and the issue-body write, then continue.
2. Follow `.claude/skills/plan/SKILL.md` through step 13. If it runs Split, stop.

## Build (the `builder` subagent)

3. Start the `builder` subagent with the issue number and the tier. It follows `/implement` and `/pr`.
4. When it returns a question, ask the owner, then send the answer to the same subagent so it keeps its context. When it returns a deviation, run the plan-mode round of `/implement` step 9 in this session, then send the outcome to the subagent. When it returns the PR link, go to Review.

## Review

5. Run the checks of `.claude/skills/review/SKILL.md` in a read-only subagent (`general-purpose` type, model `opus`) that gets only the issue number. Show its table to the owner.
6. Post the comments the owner chooses on the PR.
7. Send the owner's decisions to the same `builder` subagent, which follows `.claude/skills/feedback/SKILL.md`.
8. List the criteria in the issue body marked `Evidence: manual` or `Evidence: after merge`. Stop. The owner runs each manual check and ticks its box in the issue body, then merges. After the merge, the owner ticks each `after merge` criterion that holds.

## Keep the context small

- Do not read code, diffs, or build output in this session; the subagents do that.
- Show a subagent's result in a few lines, not in full.
