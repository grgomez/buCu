#!/usr/bin/env python3
"""PreToolUse hook: enforce the fixed SDD workflow rules.

Reads the hook JSON from stdin. Exit 2 with a reason on stderr blocks the call.
"""

import json
import os
import re
import subprocess
import sys

# A command starts a line or follows ; & | ( and optional VAR=value words.
_START = r"(?:^|[;&|(\n])\s*(?:\w+=\S*\s+)*"
_GIT = _START + r"git(?:\s+-[Cc]\s+\S+|\s+--\S+)*\s+"
_COMMIT = re.compile(_GIT + r"commit\b")
_PUSH = re.compile(_GIT + r"push\b([^;&|\n]*)")
_GH_LABELLED = re.compile(_START + r"gh\s+(?:issue|pr)\s+(?:edit|create)\b")
_ADD_LABEL = re.compile(r"(?:--add-label|--label|-l)(?:=|\s+)(?:\"([^\"]*)\"|'([^']*)'|(\S+))")
_GH_API = re.compile(_START + r"gh\s+api\b")
_PROTECTED = {"approved", "spec-approved"}
_BRANCH = re.compile(r"^(?:feat|fix)/issue-(\d+)-")
_GATED_DIRS = {"src", "include", "tests", "benchmarks", "third_party"}
_REPO = "grgomez/buCu"


def current_branch(cwd=None):
    out = subprocess.run(
        ["git", "branch", "--show-current"],
        capture_output=True, text=True, cwd=cwd, check=False,
    )
    return out.stdout.strip()


def issue_labels(issue):
    """Return the label names of issue `issue`; raise when gh fails."""
    out = subprocess.run(
        ["gh", "issue", "view", str(issue), "--repo", _REPO,
         "--json", "labels", "-q", ".labels[].name"],
        capture_output=True, text=True, check=True,
    )
    return out.stdout.split()


def repo_of(path):
    """Return (top-level directory, branch) of the repository holding `path`.

    Uses the nearest existing directory of the path, so a file in a worktree or in a
    directory not yet created resolves correctly. Raises when git fails.
    """
    directory = os.path.dirname(os.path.abspath(path))
    while not os.path.isdir(directory):
        directory = os.path.dirname(directory)

    def git(*args):
        out = subprocess.run(["git", "-C", directory, *args],
                             capture_output=True, text=True, check=True)
        return out.stdout.strip()

    return git("rev-parse", "--show-toplevel"), git("branch", "--show-current")


def _is_gated(path, root):
    """True when `path` is under a protected path of the repository `root`."""
    rel = os.path.relpath(path, root)
    parts = rel.split(os.sep)
    if parts[0] == "..":
        return False
    return parts == ["CMakeLists.txt"] or (len(parts) > 1 and parts[0] in _GATED_DIRS)


def check_approval_gate(tool_input, repo, labels):
    """Return a reason when an edit needs the approved label and lacks it."""
    path = tool_input.get("file_path") or tool_input.get("notebook_path")
    if not path:
        return None
    path = os.path.abspath(path)
    try:
        root, branch = repo(path)
    except (OSError, subprocess.SubprocessError):
        # Fail closed for any file inside the project; a file outside it is not ours.
        if os.path.relpath(path, os.getcwd()).split(os.sep)[0] == "..":
            return None
        return ("Editing code is blocked: could not find the git repository of "
                f"{path}, and the gate fails closed.")
    if not _is_gated(path, os.path.realpath(root)) and not _is_gated(path, root):
        return None
    match = _BRANCH.match(branch)
    if not match:
        return ("Editing code is blocked: the branch name has no issue number "
                "(feat|fix/issue-<n>-<slug>), so the approved label cannot be checked.")
    issue = int(match.group(1))
    try:
        names = labels(issue)
    except (OSError, subprocess.SubprocessError):
        return (f"Editing code is blocked: could not read the labels of issue #{issue} "
                "(gh failed or is offline), and the gate fails closed.")
    if "approved" not in names:
        return f"Editing code is blocked: issue #{issue} has no approved label. Run /plan {issue}."
    return None


def check_master_guard(command, branch):
    """Return a reason when `command` commits or pushes to master."""
    if _COMMIT.search(command) and branch() == "master":
        return "git commit on master is blocked. Work on an issue branch (tools/start.sh)."
    for match in _PUSH.finditer(command):
        words = [w for w in match.group(1).split() if not w.startswith("-")]
        refspecs = words[1:]
        for refspec in refspecs:
            source, _, target = refspec.lstrip("+").partition(":")
            for name in (source, target or source):
                if name in ("HEAD", "@"):
                    name = branch()
                if name.removeprefix("refs/heads/") == "master":
                    return "git push to master is blocked. The owner merges pull requests."
        if not refspecs and branch() == "master":
            return "git push from master is blocked. The owner merges pull requests."
    return None


def check_protected_labels(command):
    """Return a reason when `command` adds a label only the hook may add."""
    if _GH_LABELLED.search(command):
        for groups in _ADD_LABEL.findall(command):
            value = "".join(groups)
            if _PROTECTED & {v.strip() for v in value.split(",")}:
                return "Adding approved or spec-approved is blocked. Only plan-mode acceptance adds them."
    if _GH_API.search(command) and "/labels" in command:
        if re.search(r"(?<![\w-])(?:spec-)?approved(?![\w-])", command):
            return "Adding approved or spec-approved is blocked. Only plan-mode acceptance adds them."
    return None


def decide(payload, branch=current_branch, labels=issue_labels, repo=repo_of):
    """Return the reason to block the tool call in `payload`, or None."""
    tool = payload.get("tool_name")
    tool_input = payload.get("tool_input") or {}
    if tool == "Bash":
        command = tool_input.get("command") or ""
        return check_master_guard(command, branch) or check_protected_labels(command)
    if tool in ("Edit", "Write", "NotebookEdit"):
        return check_approval_gate(tool_input, repo, labels)
    return None


def run(raw, branch=current_branch, labels=issue_labels, repo=repo_of):
    """Return (exit code, stderr text) for the hook input `raw`."""
    try:
        payload = json.loads(raw)
    except ValueError:
        return 0, ""
    reason = decide(payload, branch=branch, labels=labels, repo=repo)
    return (2, reason + "\n") if reason else (0, "")


def main():
    code, message = run(sys.stdin.read())
    if message:
        sys.stderr.write(message)
    return code


if __name__ == "__main__":
    sys.exit(main())
