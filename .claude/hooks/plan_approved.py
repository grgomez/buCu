#!/usr/bin/env python3
"""PostToolUse hook on ExitPlanMode: label the issue when the owner accepts a plan.

The accepted plan starts with `Issue: #<n>`, `Stage: Spec|Plan`, `Tier: 0|1|2`.
Stage Spec adds `spec-approved`, Stage Plan adds `approved`, and the tier label is
set. Exit 2 puts stderr in front of Claude, so /plan stops when labelling fails.
"""

import json
import re
import subprocess
import sys

_REPO = "grgomez/buCu"
_ISSUE = re.compile(r"^Issue:\s*#(\d+)\s*$", re.M)
_STAGE = re.compile(r"^Stage:\s*(Spec|Plan)\s*$", re.M)
_TIER = re.compile(r"^Tier:\s*([012])\s*$", re.M)
_LABEL = {"Spec": "spec-approved", "Plan": "approved"}


def run_gh(args):
    subprocess.run(["gh", *args], capture_output=True, text=True, check=True)


def plan_text(response):
    """Return the accepted plan: tool_response.plan, else the file at filePath."""
    if isinstance(response, str):
        return response
    if not isinstance(response, dict):
        return None
    if response.get("plan"):
        return response["plan"]
    path = response.get("filePath")
    if path:
        try:
            with open(path, encoding="utf-8") as handle:
                return handle.read()
        except OSError:
            return None
    return None


def parse_header(text):
    """Return (issue, stage, tier) or None when the header is missing or invalid."""
    issue, stage, tier = _ISSUE.search(text), _STAGE.search(text), _TIER.search(text)
    if not (issue and stage and tier):
        return None
    return int(issue.group(1)), stage.group(1), int(tier.group(1))


def run(raw, gh=run_gh):
    """Return (exit code, stderr text) for the hook input `raw`."""
    try:
        payload = json.loads(raw)
    except ValueError:
        return 0, ""
    response = payload.get("tool_response")
    if isinstance(response, dict) and response.get("isAgent") is True:
        return 0, ""  # a subagent's plan-mode exit is not the owner's acceptance
    text = plan_text(response)
    if text is None:
        return 2, ("plan_approved: the hook input carries no plan text, so no label was added. "
                   "Ask the owner to add the labels by hand (ADR 0003).\n")
    header = parse_header(text)
    if header is None:
        return 0, ""
    issue, stage, tier = header
    args = ["issue", "edit", str(issue), "--repo", _REPO,
            "--add-label", _LABEL[stage], "--add-label", f"tier:{tier}"]
    for other in (n for n in (0, 1, 2) if n != tier):
        args += ["--remove-label", f"tier:{other}"]
    try:
        gh(args)
    except (OSError, subprocess.SubprocessError) as error:
        return 2, (f"plan_approved: could not label issue #{issue} ({error}). "
                   "Ask the owner to add the labels by hand (ADR 0003).\n")
    return 0, ""


def main():
    code, message = run(sys.stdin.read())
    if message:
        sys.stderr.write(message)
    return code


if __name__ == "__main__":
    sys.exit(main())
