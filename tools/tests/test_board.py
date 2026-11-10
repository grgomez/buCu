"""Tests for tools/board.sh, with a fake `gh` first on PATH."""

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BOARD = ROOT / "tools" / "board.sh"

# Answers by command; each test changes them through the FAKE_GH_* variables.
FAKE_GH = r'''#!/bin/sh
echo "$*" >> "$FAKE_GH_LOG"
case "$1 $2" in
    "project view") echo "${FAKE_GH_PROJECT_ID-PVT_1}" ;;
    "project field-list")
        echo "${FAKE_GH_FIELD_ID-FLD_1}"
        [ -z "${FAKE_GH_OPTION_ID-OPT_1}" ] || echo "${FAKE_GH_OPTION_ID-OPT_1}" ;;
    "project item-list") echo "${FAKE_GH_ITEM_ID-ITEM_1}" ;;
    "project item-edit") [ -z "${FAKE_GH_EDIT_FAIL-}" ] || { echo "edit failed" >&2; exit 1; } ;;
    "issue view")
        case "$*" in
            *title*) echo "${FAKE_GH_TITLE-Add the thing}" ;;
            *labels*) printf '%s' "${FAKE_GH_LABELS-}" ;;
        esac ;;
    *) echo "fake gh: unexpected $*" >&2; exit 1 ;;
esac
'''


class FakeGhCase(unittest.TestCase):
    """A bin directory with the fake gh first on PATH, and a call log."""

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmp = Path(self._tmp.name)
        self.bin = self.tmp / "bin"
        self.bin.mkdir()
        gh = self.bin / "gh"
        gh.write_text(FAKE_GH)
        gh.chmod(0o755)
        self.log = self.tmp / "gh.log"

    def env(self, **extra):
        env = dict(os.environ, FAKE_GH_LOG=str(self.log))
        env["PATH"] = str(self.bin) + os.pathsep + env["PATH"]
        env.update(extra)
        return env

    def calls(self):
        return self.log.read_text().splitlines() if self.log.exists() else []


class BoardTest(FakeGhCase):
    def board(self, *args, **env):
        return subprocess.run([shutil.which("sh"), str(BOARD), *args], capture_output=True,
                              text=True, env=self.env(**env), check=False)

    def test_moves_the_item(self):
        done = self.board("34", "In review")
        self.assertEqual(done.returncode, 0, done.stderr)
        edit = self.calls()[-1]
        self.assertEqual(edit, "project item-edit --project-id PVT_1 --id ITEM_1 "
                               "--field-id FLD_1 --single-select-option-id OPT_1")

    def test_looks_up_the_item_of_the_issue(self):
        self.board("34", "Done")
        listing = next(c for c in self.calls() if c.startswith("project item-list"))
        self.assertIn(".content.number == 34", listing)
        self.assertIn("--limit 1000", listing)

    def test_item_lookup_filters_on_the_repository(self):
        self.board("34", "Done")
        listing = next(c for c in self.calls() if c.startswith("project item-list"))
        self.assertIn('.content.repository == "grgomez/buCu"', listing)

    def test_several_items_for_the_issue(self):
        done = self.board("34", "Done", FAKE_GH_ITEM_ID="ITEM_1\nITEM_2")
        self.assertEqual(done.returncode, 1)
        self.assertIn("more than one", done.stderr)
        self.assertFalse(any(c.startswith("project item-edit") for c in self.calls()))

    def test_unknown_status(self):
        done = self.board("34", "Someday", FAKE_GH_OPTION_ID="")
        self.assertEqual(done.returncode, 2)
        self.assertIn("unknown status", done.stderr)
        self.assertFalse(any(c.startswith("project item-edit") for c in self.calls()))

    def test_status_with_a_quote_is_rejected_before_gh_runs(self):
        done = self.board("34", 'Done" | x')
        self.assertEqual(done.returncode, 2)
        self.assertEqual(self.calls(), [])

    def test_issue_not_on_the_board(self):
        done = self.board("34", "Done", FAKE_GH_ITEM_ID="")
        self.assertEqual(done.returncode, 1)
        self.assertIn("not found", done.stderr)
        self.assertFalse(any(c.startswith("project item-edit") for c in self.calls()))

    def test_edit_failure(self):
        done = self.board("34", "Done", FAKE_GH_EDIT_FAIL="1")
        self.assertEqual(done.returncode, 1)
        self.assertIn("could not move", done.stderr)

    def test_usage(self):
        for args in ((), ("34",), ("x", "Done"), ("34", "Done", "more")):
            self.assertEqual(self.board(*args).returncode, 2, args)


if __name__ == "__main__":
    unittest.main()
