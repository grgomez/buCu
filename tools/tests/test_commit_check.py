"""Tests for tools/commit_check.py and the commit-msg hook."""

import importlib.util
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("commit_check", ROOT / "tools" / "commit_check.py")
commit_check = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(commit_check)


class SubjectTest(unittest.TestCase):
    def test_accepted_subjects(self):
        for subject in (
            "T001 Block git commit on master",
            "T123 x",
            "R1 Fix the review finding",
            "R12 Another one",
            "Merge pull request #33 from grgomez/feat/issue-1-x",
            "Merge branch 'master' into feat/issue-2-x",
            'Revert "T001 Block git commit"',
            "fixup! T001 Block git commit",
            "squash! T002 Block labels",
        ):
            self.assertTrue(commit_check.valid_subject(subject), subject)

    def test_rejected_subjects(self):
        for subject in (
            "",
            "Add a hook",
            "feat: add a hook",
            "T1 too few digits",
            "T0001 too many digits",
            "t001 lowercase",
            "T001",
            "T001  ",
            "T001Block no space",
            "R Block",
            "Rx1 Block",
            "Merged things",
        ):
            self.assertFalse(commit_check.valid_subject(subject), repr(subject))

    def test_subject_is_the_first_line_that_is_not_a_comment(self):
        self.assertEqual(
            commit_check.subject_of("# comment\n\nT001 Real subject\n\nbody"), "T001 Real subject"
        )


class CommitMsgHookTest(unittest.TestCase):
    def run_hook(self, message):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "COMMIT_EDITMSG"
            path.write_text(message)
            return subprocess.run(
                ["sh", str(ROOT / ".githooks" / "commit-msg"), str(path)],
                capture_output=True, text=True, check=False,
            )

    def test_hook_accepts_a_task_subject(self):
        self.assertEqual(self.run_hook("T001 Do it\n\nbody\n").returncode, 0)

    def test_hook_rejects_another_subject_and_says_why(self):
        result = self.run_hook("fix stuff\n")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("T<NNN>", result.stderr)


if __name__ == "__main__":
    unittest.main()
