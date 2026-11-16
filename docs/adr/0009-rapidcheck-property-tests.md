# ADR 0009: RapidCheck for property tests

- Status: Accepted
- Date: 2026-11-15
- Issue: #65

## Context

[ADR 0008](0008-evidence-kinds.md) made a property test the test of constitution principle 4. The pure functions of `core` (UTF-8, lowercasing, tags, URLs, flags) have rules in product §4 that hold for every input, and example tests cover only the inputs someone thought of. Catch2 has generators but no shrinking, and no way to state a property over generated structures. The build must stay offline once the sources are present ([ADR 0004](0004-vendored-dependencies.md)).

## Considered options

- **RapidCheck, vendored at a pinned commit (chosen):** generators that combine and shrink, and a Catch2 integration header. It has no tagged releases and its CMake file forces C++11, so we pin a commit and write our own CMake file.
- **Catch2 generators alone:** nothing new to vendor, but no shrinking and no generation of structured input, so counterexamples are hard to read.
- **Fetch RapidCheck at configure time:** a smaller repository, but the first build needs the network (ADR 0004).
- **A fuzzer for everything:** finds crashes, but states no property and has no red step in `ctest`.

## Decision

We will vendor RapidCheck (BSD-2-Clause) in `third_party/rapidcheck/` at commit `2c3c4365aca21ef4e612768fdc95b1ce8b39a651`, keeping `include/`, `src/`, the Catch integration header, `LICENSE.md`, and `VERSION.txt`. Our own `CMakeLists.txt` builds the static target `bucu::rapidcheck` with SYSTEM includes and no project warning flags. `scripts/update-deps.sh` pins the commit archive by SHA256 and refreshes it.

Properties live in a second `core` test executable, `bucu_core_property_tests`, which ctest runs as `core-property` with a fixed `RC_PARAMS` seed, so CI is repeatable. Running the binary with another `RC_PARAMS` explores other seeds. Only tests link RapidCheck; `core` does not.

## Consequences

- Each property cites the rule of product §4 it checks, and a failure prints a shrunk counterexample.
- A fixed seed explores the same inputs on every run. Other seeds are run by hand when a rule changes.
- The library adds a few seconds to a parallel build. MSVC is built only by the CI Windows job.
- A new commit pin is a deliberate change to `scripts/update-deps.sh`.
- A property of a function that already exists passes at once, so its red step is a recorded mutation check, not a failing commit.
