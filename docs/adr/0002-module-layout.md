# ADR 0002: Six library modules

- Status: Accepted
- Date: 2026-11-09
- Issue: #1

## Context

BuCu must stay maintainable for a long time and portable across operating systems. Platform behaviour (paths, browser, editor, terminal) and networking must not leak into the domain logic or the database layer.

## Considered options

- **One library plus the executable:** simplest, but nothing stops a platform header or SQL in the domain code.
- **Six CMake library targets with a fixed dependency direction (chosen):** core, db, io, net, platform, cli; the target graph enforces the boundaries.
- **Separate repositories or packages:** too much overhead for one program.

## Decision

We will build six static library targets, `core`, `db`, `io`, `net`, `platform`, and `cli`, plus the `bucu` executable. Their responsibilities, dependency direction, and MUST NOT rules are in architecture §3 and §4. `platform` and `net` are interfaces that `cli` implements and injects, so tests use fakes.

## Consequences

- A boundary violation fails to link, not just a review.
- A few interfaces exist from the start; constitution principle 2 allows them because portability requires them.
- Moving a responsibility needs a new ADR.
