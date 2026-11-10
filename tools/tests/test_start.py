"""Tests for tools/start.sh: a throwaway repository with a bare origin, and a fake gh."""

import importlib.util
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
START = ROOT / "tools" / "start.sh"
_SPEC = importlib.util.spec_from_file_location("test_board", ROOT / "tools" / "tests" / "test_board.py")
test_board = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(test_board)


def git(repo, *args):
    return subprocess.run(["git", "-C", str(repo), *args], capture_output=True, text=True,
                          check=True).stdout.strip()


class StartTest(test_board.FakeGhCase):
    def setUp(self):
        super().setUp()
        self.origin = self.tmp / "origin.git"
        subprocess.run(["git", "init", "-q", "--bare", "-b", "master", str(self.origin)], check=True)
        self.repo = self.tmp / "repo"
        subprocess.run(["git", "clone", "-q", str(self.origin), str(self.repo)],
                       capture_output=True, check=True)
        git(self.repo, "config", "user.email", "t@example.com")
        git(self.repo, "config", "user.name", "T")
        (self.repo / "a.txt").write_text("a\n")
        git(self.repo, "add", "a.txt")
        git(self.repo, "commit", "-q", "-m", "first")
        git(self.repo, "push", "-q", "origin", "master")

    def start(self, *args, **env):
        return subprocess.run([shutil.which("sh"), str(START), *args], cwd=self.repo,
                              capture_output=True, text=True, env=self.env(**env), check=False)

    def push_master_commit(self):
        """Add a commit to origin/master that the local master does not have."""
        other = self.tmp / "other"
        subprocess.run(["git", "clone", "-q", str(self.origin), str(other)],
                       capture_output=True, check=True)
        git(other, "config", "user.email", "t@example.com")
        git(other, "config", "user.name", "T")
        (other / "b.txt").write_text("b\n")
        git(other, "add", "b.txt")
        git(other, "commit", "-q", "-m", "second")
        git(other, "push", "-q", "origin", "master")
        return git(other, "rev-parse", "HEAD")

    def test_creates_the_branch_from_origin_master_with_no_upstream(self):
        newest = self.push_master_commit()
        done = self.start("34")
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertEqual(git(self.repo, "branch", "--show-current"), "feat/issue-34-add-the-thing")
        self.assertEqual(git(self.repo, "rev-parse", "HEAD"), newest)
        self.assertEqual(subprocess.run(
            ["git", "-C", str(self.repo), "config", "branch.feat/issue-34-add-the-thing.remote"],
            capture_output=True, text=True).returncode, 1)

    def test_moves_the_board_item_to_in_progress(self):
        self.start("34")
        listing = next(c for c in self.calls() if c.startswith("project field-list") and '.options' in c)
        self.assertIn('"In progress"', listing)
        self.assertTrue(self.calls()[-1].startswith("project item-edit"))

    def test_bug_label_makes_a_fix_branch(self):
        self.start("34", FAKE_GH_LABELS="enhancement\nbug\n")
        self.assertEqual(git(self.repo, "branch", "--show-current"), "fix/issue-34-add-the-thing")

    def test_escaped_label_makes_a_fix_branch(self):
        self.start("34", FAKE_GH_LABELS="escaped\n")
        self.assertTrue(git(self.repo, "branch", "--show-current").startswith("fix/issue-34-"))

    def test_slug(self):
        cases = {
            "Add `--offline` flag (v1.0)!": "add-offline-flag-v1-0",
            "  Leading and trailing  ": "leading-and-trailing",
            "Give the SDD workflow its scripts and pull request gates": "give-the-sdd-workflow-its-scripts-and",
            "Exactly forty characters long title ok": "exactly-forty-characters-long-title-ok",
            "x" * 60: "x" * 40,
            "!!!": "work",
        }
        for number, (title, slug) in enumerate(cases.items(), start=100):
            with self.subTest(title=title):
                done = self.start(str(number), FAKE_GH_TITLE=title)
                self.assertEqual(done.returncode, 0, done.stderr)
                self.assertEqual(git(self.repo, "branch", "--show-current"),
                                 f"feat/issue-{number}-{slug}")
                self.assertLessEqual(len(slug), 40)

    def test_resumes_a_local_branch(self):
        git(self.repo, "switch", "-q", "-c", "feat/issue-34-old-name")
        git(self.repo, "switch", "-q", "master")
        done = self.start("34")
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertEqual(git(self.repo, "branch", "--show-current"), "feat/issue-34-old-name")
        self.assertFalse(any(c.startswith("issue view") for c in self.calls()))

    def test_resumes_a_branch_only_on_origin(self):
        git(self.repo, "switch", "-q", "-c", "fix/issue-34-remote-one")
        git(self.repo, "push", "-q", "origin", "fix/issue-34-remote-one")
        git(self.repo, "switch", "-q", "master")
        git(self.repo, "branch", "-q", "-D", "fix/issue-34-remote-one")
        done = self.start("34")
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertEqual(git(self.repo, "branch", "--show-current"), "fix/issue-34-remote-one")

    def test_issue_number_prefix_is_exact(self):
        git(self.repo, "switch", "-q", "-c", "feat/issue-340-other")
        git(self.repo, "switch", "-q", "master")
        self.start("34")
        self.assertEqual(git(self.repo, "branch", "--show-current"), "feat/issue-34-add-the-thing")

    def test_refuses_a_dirty_tree(self):
        (self.repo / "a.txt").write_text("changed\n")
        done = self.start("34")
        self.assertEqual(done.returncode, 1)
        self.assertIn("uncommitted", done.stderr)
        self.assertEqual(git(self.repo, "branch", "--show-current"), "master")

    def test_board_failure_exits_3_with_the_branch_ready(self):
        done = self.start("34", FAKE_GH_EDIT_FAIL="1")
        self.assertEqual(done.returncode, 3)
        self.assertIn("move the board item by hand", done.stderr)
        self.assertEqual(git(self.repo, "branch", "--show-current"), "feat/issue-34-add-the-thing")

    def test_usage(self):
        for args in ((), ("x",), ("34", "35")):
            self.assertEqual(self.start(*args).returncode, 2, args)


if __name__ == "__main__":
    unittest.main()
