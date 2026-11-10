#!/usr/bin/env python3
"""PreToolUse hook: enforce the fixed SDD workflow rules.

Reads the hook JSON from stdin. Exit 2 with a reason on stderr blocks the call.
"""

import json
import re
import subprocess
import sys

# A command starts a line or follows ; & | ( and optional VAR=value words.
_START = r"(?:^|[;&|(\n])\s*(?:\w+=\S*\s+)*"
_GIT = _START + r"git(?:\s+-[Cc]\s+\S+|\s+--\S+)*\s+"
_COMMIT = re.compile(_GIT + r"commit\b")
_PUSH = re.compile(_GIT + r"push\b([^;&|\n]*)")
_MASTER_REF = re.compile(r"^\+?(?:[^:]*:)?(?:refs/heads/)?master$")


def current_branch(cwd=None):
    out = subprocess.run(
        ["git", "branch", "--show-current"],
        capture_output=True, text=True, cwd=cwd, check=False,
    )
    return out.stdout.strip()


def check_master_guard(command, branch):
    """Return a reason when `command` commits or pushes to master."""
    if _COMMIT.search(command) and branch() == "master":
        return "git commit on master is blocked. Work on an issue branch (tools/start.sh)."
    for match in _PUSH.finditer(command):
        words = [w for w in match.group(1).split() if not w.startswith("-")]
        refspecs = words[1:]
        if any(_MASTER_REF.match(r) for r in refspecs):
            return "git push to master is blocked. The owner merges pull requests."
        if not refspecs and branch() == "master":
            return "git push from master is blocked. The owner merges pull requests."
    return None


def decide(payload, branch=current_branch):
    """Return the reason to block the tool call in `payload`, or None."""
    tool = payload.get("tool_name")
    tool_input = payload.get("tool_input") or {}
    if tool == "Bash":
        command = tool_input.get("command") or ""
        return check_master_guard(command, branch)
    return None


def run(raw, branch=current_branch):
    """Return (exit code, stderr text) for the hook input `raw`."""
    try:
        payload = json.loads(raw)
    except ValueError:
        return 0, ""
    reason = decide(payload, branch=branch)
    return (2, reason + "\n") if reason else (0, "")


def main():
    code, message = run(sys.stdin.read())
    if message:
        sys.stderr.write(message)
    return code


if __name__ == "__main__":
    sys.exit(main())
