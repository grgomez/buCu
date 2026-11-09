# Plan

## Technical approach

How the change will be built, in a few short paragraphs. Name the modules that change and why each one owns its part (`docs/architecture.md` §4).

## Trials

One row per third-party unknown (`/plan` Trial). Facts only. Number the rows `TR1`, `TR2`. With no unknown, replace the table with one line: "No trial needed", and the reason.

| ID | Unknown | What was tried | Versions | OS | Attempts | Result | Decisions |
| --- | --- | --- | --- | --- | --- | --- | --- |
| TR1 | <the fact the plan depends on> | <prose> | <tool and compiler versions> | <operating system> | <1 to 3> | <what was observed> | <D-IDs that rest on it> |

## Decisions

One row per decision, one short line in each column. Cite the Spec part's IDs (for example `US1-AC2`, `FR-003`); never restate them. Start the Reason of a decision that rests on a trial with its trial ID. Write a decision only when a reasonable person could choose differently.

| # | Decision | Reason | Risk |
| --- | --- | --- | --- |
| D1 | <what> | <why this option> | <what could go wrong, and the mitigation> |

## Files to change

List every file the plan changes except the living documents.

- `<path>`: <change>

## Contract records

Write this section only when the schema, an export format, or a public header in `include/bucu/` changes (`docs/architecture.md` §5). Implement writes each record from [`docs/contracts/0000-template.md`](../../../docs/contracts/0000-template.md).

- New: `docs/contracts/<kind>-<NNN>-<title>.md` (<what changes, for example schema version 1 to 2>)
- Previous: <links to the latest record of each kind this plan changes, or "none">

## Manual checks

Write this section only when the change has behaviour the owner can check by hand that no automated check proves (for example opening a real browser or editor).

1. <AC ID>: <command>. Expected: <result>.

## Analyze (Tier 2)

The `/plan` Analyze table, with the owner's decision for each finding: fix, drop, or new issue.

## Constitution check

Complies with `docs/constitution.md`. List any exception, with its reason, under Decisions.

## ADRs

- New: `docs/adr/NNNN-<title>.md` (only when the change makes a major decision)
- Related: <links to the existing ADRs this plan depends on, or "none">

## Documentation updates

Approved with the plan. One line each: a link to the owner document's section, the change in a few words, and its decision ID.

- [`<owner document>` §<section>](<relative link>): <the change> (D<n>)
