#!/usr/bin/env python3
"""Check the SDD issue body and pull request: `sdd_check.py issue <n>` or `pr <n>`.

The checks are functions over strings; `gh` is read through one runner that the
tests replace. Exit 0 when every check passes, 1 with one line per failure, 2 on
a usage or `gh` error.
"""

import re

_MARKER = re.compile(r"^<!-- (SPEC|PLAN|TASKS):(START|END) -->\s*$")
_TIERS = ("tier:0", "tier:1", "tier:2")
_PARTS = ("SPEC", "PLAN", "TASKS")


def find_markers(body):
    """Return [(line index, 'PLAN:START'), ...] for markers alone on their line."""
    found = []
    for index, line in enumerate(body.splitlines()):
        match = _MARKER.match(line)
        if match:
            found.append((index, f"{match.group(1)}:{match.group(2)}"))
    return found


def split_parts(body):
    """Return {'SPEC': text, ...} for each part with one START and one END marker."""
    lines = body.splitlines()
    marks = find_markers(body)
    parts = {}
    for name in _PARTS:
        starts = [i for i, m in marks if m == f"{name}:START"]
        ends = [i for i, m in marks if m == f"{name}:END"]
        if len(starts) == 1 and len(ends) == 1 and starts[0] < ends[0]:
            parts[name] = "\n".join(lines[starts[0] + 1:ends[0]])
    return parts


def tier_of(names):
    """Return the tier number from exactly one tier:<n> label, else None."""
    tiers = [name for name in names if name in _TIERS]
    return int(tiers[0][-1]) if len(tiers) == 1 else None


def check_parts(body, names):
    """Return the failures of the layout in `.claude/skills/plan/issue-body.md` and the labels."""
    failures = []
    tier = tier_of(names)
    if tier is None:
        failures.append("the issue needs exactly one tier label (tier:0, tier:1, or tier:2)")
    if "approved" not in names:
        failures.append("the issue has no approved label")
    if tier == 2 and "spec-approved" not in names:
        failures.append("a Tier 2 issue needs the spec-approved label")

    marks = find_markers(body)
    seen = [m for _, m in marks]
    for name in _PARTS:
        for kind in ("START", "END"):
            count = seen.count(f"{name}:{kind}")
            if count > 1:
                failures.append(f"marker {name}:{kind} appears {count} times, expected once")
    if all(seen.count(m) <= 1 for m in seen):
        expected = [f"{n}:{k}" for n in _PARTS for k in ("START", "END") if f"{n}:START" in seen
                    or f"{n}:END" in seen]
        if seen != expected:
            failures.append("the part markers are not in the order SPEC, PLAN, TASKS "
                            "with each START before its END")

    parts = split_parts(body)
    for name in ("PLAN", "TASKS"):
        if f"{name}:START" not in seen or f"{name}:END" not in seen:
            failures.append(f"the {name} part is missing: it needs a {name}:START and a "
                            f"{name}:END marker, each alone on its line")
    for name, text in parts.items():
        if not text.strip():
            failures.append(f"the {name} part is empty")
    if tier is not None:
        has_spec = "SPEC:START" in seen or "SPEC:END" in seen
        if tier == 0 and has_spec:
            failures.append("a Tier 0 issue must not have a Spec part")
        if tier in (1, 2) and not has_spec:
            failures.append(f"a Tier {tier} issue needs a Spec part")

    if marks:
        before = body.splitlines()[:marks[0][0]]
        last = next((line for line in reversed(before) if line.strip()), "")
        if last.strip() != "---":
            failures.append("a `---` line must come right before the first part marker")
    return failures
