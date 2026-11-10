#!/usr/bin/env python3
"""Check the SDD issue body and pull request: `sdd_check.py issue <n>` or `pr <n>`.

The checks are functions over strings; `gh` is read through one runner that the
tests replace. Exit 0 when every check passes, 1 with one line per failure, 2 on
a usage or `gh` error.
"""

import json
import re
import subprocess
import sys

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


_LIMITS = (("user stories", 3), ("criteria", 12), ("tasks", 12))
_STORY = re.compile(r"^#{2,3} US\d+\b", re.M)
_CRITERION = re.compile(r"^- \[[ xX]\] US\d+-AC\d+:", re.M)
_TASK = re.compile(r"^- \[[ xX]\] T\d{3}\b", re.M)
_OPEN_MARKER = re.compile(r"\[NEEDS CLARIFICATION:")


def original_text(body):
    """Return the owner's text: everything before the `---` line and the first marker."""
    lines = body.splitlines()
    marks = find_markers(body)
    if not marks:
        return body
    head = lines[:marks[0][0]]
    while head and not head[-1].strip():
        head.pop()
    if head and head[-1].strip() == "---":
        head.pop()
    return "\n".join(head)


def scope_bullets(body):
    """Return the bullet lines under `## Scope` in the owner's text."""
    bullets, inside = [], False
    for line in original_text(body).splitlines():
        if line.startswith("## "):
            inside = line.strip() == "## Scope"
        elif inside and re.match(r"\s*- ", line):
            bullets.append(line)
    return bullets


def check_size(body):
    """Return the failures of the size limit and of leftover clarification markers.

    Without a Spec part (Tier 0), the criteria are the Scope bullets and the user
    stories are the `## US<n>` headings of the Tasks part.
    """
    parts = split_parts(body)
    if "SPEC" in parts:
        counts = (len(_STORY.findall(parts["SPEC"])), len(_CRITERION.findall(parts["SPEC"])))
    else:
        counts = (len(_STORY.findall(parts.get("TASKS", ""))), len(scope_bullets(body)))
    counts += (len(_TASK.findall(parts.get("TASKS", ""))),)
    failures = [f"{count} {name}, the limit is {limit}"
                for (name, limit), count in zip(_LIMITS, counts) if count > limit]
    for name, text in parts.items():
        if _OPEN_MARKER.search(text):
            failures.append(f"the {name} part has an open [NEEDS CLARIFICATION: ...] marker")
    return failures


_CLOSES = re.compile(r"^\s*Closes #(\d+)\b", re.M | re.I)
_CHECKBOX = re.compile(r"^\s*- \[([ xX])\] ")
_UNTICKED_TASK = re.compile(r"^- \[ \] (T\d{3})\b", re.M)
_OPEN_EVIDENCE = re.compile(r"Evidence: (?:manual|after merge)")


def closing_issue(pr_body):
    """Return the issue number of the `Closes #<n>` line, or None."""
    match = _CLOSES.search(pr_body)
    return int(match.group(1)) if match else None


def criteria_lines(body, tier):
    """Return the acceptance criteria lines: the Spec part's, or the Scope bullets for Tier 0."""
    if tier == 0:
        return scope_bullets(body)
    return [line for line in split_parts(body).get("SPEC", "").splitlines()
            if _CRITERION.match(line)]


def check_done(body, names):
    """Return the done failures of constitution section 3: tasks ticked, criteria with evidence."""
    failures = []
    tasks = split_parts(body).get("TASKS", "")
    for task in _UNTICKED_TASK.findall(tasks):
        failures.append(f"task {task} is not ticked")
    for line in criteria_lines(body, tier_of(names)):
        label = line.strip()[:60]
        box = _CHECKBOX.match(line)
        if not box:
            failures.append(f"criterion has no checkbox: {label}")
        elif box.group(1) == " ":
            if not _OPEN_EVIDENCE.search(line):
                failures.append(f"unticked criterion needs `Evidence: manual` or "
                                f"`Evidence: after merge`: {label}")
        elif "Evidence:" not in line:
            failures.append(f"ticked criterion has no `Evidence:`: {label}")
    return failures


_LIVING_FILES = ("CLAUDE.md", "README.md")
_LIVING_DIRS = ("docs/", ".claude/skills/", ".claude/agents/")


def is_living_document(path):
    """True when `path` is a living document (`CLAUDE.md` Documentation map)."""
    return path in _LIVING_FILES or path.startswith(_LIVING_DIRS)


def living_documents_section(pr_body):
    """Return the text under `## Living documents` up to the next `## ` heading, or None."""
    lines, section, inside = pr_body.splitlines(), [], False
    for line in lines:
        if line.startswith("## "):
            if inside:
                break
            inside = line.strip().lower() == "## living documents"
        elif inside:
            section.append(line)
    return "\n".join(section) if inside else None


def check_living_documents(changed, pr_body, body):
    """Return the failures for the PR's Living documents section and each changed document."""
    section = living_documents_section(pr_body)
    if section is None:
        return ["the PR body has no `## Living documents` section"]
    plan = split_parts(body).get("PLAN", "")
    failures = []
    for path in (p for p in changed if is_living_document(p)):
        if path not in section:
            failures.append(f"{path} changed but is not in the PR's Living documents section")
        if path not in plan:
            failures.append(f"{path} changed but is not named in the Plan part")
    return failures


_REPO = "grgomez/buCu"
_USAGE = "usage: sdd_check.py issue <n> | pr <n>\n"


def run_gh(args):
    """Run `gh` and return its stdout."""
    return subprocess.run(["gh", *args], capture_output=True, text=True, check=True).stdout


def read_issue(number, gh):
    """Return (body, label names) of an issue."""
    data = json.loads(gh(["issue", "view", str(number), "--repo", _REPO, "--json", "body,labels"]))
    return data["body"] or "", [label["name"] for label in data["labels"]]


def check_issue(number, gh):
    """Return the `issue` failures for issue `number`."""
    body, names = read_issue(number, gh)
    return [f"issue #{number}: {f}" for f in check_parts(body, names) + check_size(body)]


def check_pr(number, gh):
    """Return the `pr` failures for pull request `number`."""
    data = json.loads(gh(["pr", "view", str(number), "--repo", _REPO, "--json", "body,files"]))
    pr_body = data["body"] or ""
    issue = closing_issue(pr_body)
    if issue is None:
        return [f"PR #{number}: the body has no `Closes #<number>` line"]
    body, names = read_issue(issue, gh)
    changed = [item["path"] for item in data["files"]]
    failures = (check_parts(body, names) + check_size(body) + check_done(body, names)
                + check_living_documents(changed, pr_body, body))
    return [f"PR #{number} (issue #{issue}): {f}" for f in failures]


def main(argv, gh=run_gh, err=sys.stderr):
    if len(argv) != 2 or argv[0] not in ("issue", "pr") or not argv[1].isdigit():
        err.write(_USAGE)
        return 2
    check = check_issue if argv[0] == "issue" else check_pr
    try:
        failures = check(int(argv[1]), gh)
    except (OSError, subprocess.SubprocessError, ValueError, KeyError, TypeError) as error:
        err.write(f"sdd_check: could not read from gh ({error})\n")
        return 2
    for failure in failures:
        err.write(failure + "\n")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
