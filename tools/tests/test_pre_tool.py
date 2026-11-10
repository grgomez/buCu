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


if __name__ == "__main__":
    unittest.main()
