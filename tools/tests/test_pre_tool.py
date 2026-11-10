"""Tests for .claude/hooks/pre_tool.py."""

import importlib.util
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "pre_tool", ROOT / ".claude" / "hooks" / "pre_tool.py"
)
pre_tool = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(pre_tool)


def bash(command):
    return {"tool_name": "Bash", "tool_input": {"command": command}}


class MasterGuardTest(unittest.TestCase):
    def blocked(self, command, branch):
        return pre_tool.decide(bash(command), branch=lambda: branch) is not None

    def test_commit_on_master_is_blocked(self):
        self.assertTrue(self.blocked("git commit -m x", "master"))

    def test_commit_on_issue_branch_is_allowed(self):
        self.assertFalse(self.blocked("git commit -m x", "feat/issue-2-sdd-hooks"))

    def test_commit_inside_a_compound_command_is_blocked(self):
        self.assertTrue(self.blocked("git add -A && git commit -m x", "master"))

    def test_commit_with_global_options_is_blocked(self):
        self.assertTrue(self.blocked("git -C /tmp/r commit -m x", "master"))

    def test_push_with_master_refspec_is_blocked_from_any_branch(self):
        for cmd in (
            "git push origin master",
            "git push origin HEAD:master",
            "git push origin HEAD:refs/heads/master",
            "git push -f origin +master",
        ):
            self.assertTrue(self.blocked(cmd, "feat/issue-2-x"), cmd)

    def test_bare_push_from_master_is_blocked(self):
        self.assertTrue(self.blocked("git push", "master"))
        self.assertTrue(self.blocked("git push origin", "master"))

    def test_push_of_an_issue_branch_is_allowed(self):
        self.assertFalse(self.blocked("git push -u origin feat/issue-2-x", "master"))
        self.assertFalse(self.blocked("git push", "feat/issue-2-x"))

    def test_other_commands_and_tools_are_allowed(self):
        self.assertFalse(self.blocked("git status", "master"))
        self.assertFalse(self.blocked("echo git commit", "master"))
        self.assertIsNone(
            pre_tool.decide({"tool_name": "Read", "tool_input": {}}, branch=lambda: "master")
        )

    def test_main_exit_codes(self):
        payload = json.dumps(bash("git commit -m x"))
        self.assertEqual(pre_tool.run(payload, branch=lambda: "master")[0], 2)
        self.assertEqual(pre_tool.run(payload, branch=lambda: "feat/issue-1-a")[0], 0)
        self.assertEqual(pre_tool.run("not json", branch=lambda: "master")[0], 0)


class ProtectedLabelTest(unittest.TestCase):
    def blocked(self, command):
        return pre_tool.decide(bash(command), branch=lambda: "feat/issue-2-x") is not None

    def test_adding_approved_is_blocked(self):
        for cmd in (
            "gh issue edit 2 --add-label approved",
            "gh issue edit 2 --repo grgomez/buCu --add-label spec-approved",
            "gh issue edit 2 --add-label=approved",
            'gh pr edit 3 --add-label "tier:0,approved"',
            "gh issue edit 2 --add-label 'spec-approved'",
        ):
            self.assertTrue(self.blocked(cmd), cmd)

    def test_api_label_call_naming_approved_is_blocked(self):
        self.assertTrue(
            self.blocked("gh api repos/grgomez/buCu/issues/2/labels -f 'labels[]=approved'")
        )

    def test_other_labels_and_reads_are_allowed(self):
        for cmd in (
            "gh issue edit 2 --add-label workflow",
            "gh issue edit 2 --remove-label approved",
            "gh issue view 2 --json labels",
            "gh issue edit 2 --add-label approved-by-hand-not",
            "gh api repos/grgomez/buCu/issues/2",
        ):
            self.assertFalse(self.blocked(cmd), cmd)


ROOT_DIR = "/work/repo"


def edit(path, tool="Edit"):
    key = "notebook_path" if tool == "NotebookEdit" else "file_path"
    return {"tool_name": tool, "tool_input": {key: path}}


def labels_of(*names):
    return lambda issue: list(names)


def failing_labels(issue):
    raise OSError("gh unavailable")


class ApprovalGateTest(unittest.TestCase):
    def reason(self, payload, branch="feat/issue-2-x", labels=labels_of("approved")):
        return pre_tool.decide(
            payload, branch=lambda: branch, labels=labels, root=ROOT_DIR
        )

    def test_protected_paths_need_approval(self):
        for path in (
            "src/db/open.cpp",
            "include/bucu/db.hpp",
            "tests/test_db.cpp",
            "benchmarks/run.sh",
            "third_party/catch2/x.hpp",
            "CMakeLists.txt",
        ):
            for tool in ("Edit", "Write", "NotebookEdit"):
                payload = edit(ROOT_DIR + "/" + path, tool)
                self.assertIsNotNone(self.reason(payload, labels=labels_of("workflow")), path)
                self.assertIsNone(self.reason(payload), path)

    def test_relative_paths_are_checked_too(self):
        self.assertIsNotNone(self.reason(edit("src/a.cpp"), labels=labels_of()))

    def test_other_paths_are_not_gated(self):
        for path in (
            "docs/product.md",
            "tools/tests/test_pre_tool.py",
            ".claude/hooks/pre_tool.py",
            "README.md",
            "docs/CMakeLists.txt",
            "/elsewhere/src/a.cpp",
        ):
            payload = edit(path if path.startswith("/") else ROOT_DIR + "/" + path)
            self.assertIsNone(self.reason(payload, labels=failing_labels), path)

    def test_fails_closed_without_an_issue_number(self):
        payload = edit(ROOT_DIR + "/src/a.cpp")
        self.assertIsNotNone(self.reason(payload, branch="master"))
        self.assertIsNotNone(self.reason(payload, branch=""))

    def test_fails_closed_when_gh_fails(self):
        payload = edit(ROOT_DIR + "/src/a.cpp")
        self.assertIsNotNone(self.reason(payload, labels=failing_labels))

    def test_spec_approved_alone_is_not_enough(self):
        payload = edit(ROOT_DIR + "/src/a.cpp")
        self.assertIsNotNone(self.reason(payload, labels=labels_of("spec-approved")))

    def test_issue_number_comes_from_the_branch(self):
        seen = []

        def labels(issue):
            seen.append(issue)
            return ["approved"]

        self.reason(edit(ROOT_DIR + "/src/a.cpp"), branch="fix/issue-17-bug", labels=labels)
        self.assertEqual(seen, [17])


if __name__ == "__main__":
    unittest.main()
