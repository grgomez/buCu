"""Tests for .claude/hooks/plan_approved.py."""

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "plan_approved", ROOT / ".claude" / "hooks" / "plan_approved.py"
)
plan_approved = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(plan_approved)


class Recorder:
    """A stand-in for the gh runner that records its argument lists."""

    def __init__(self, fail=False):
        self.calls = []
        self.fail = fail

    def __call__(self, args):
        self.calls.append(args)
        if self.fail:
            raise OSError("gh unavailable")


def payload(plan=None, file_path=None):
    response = {"isAgent": False}
    if plan is not None:
        response["plan"] = plan
    if file_path is not None:
        response["filePath"] = file_path
    return json.dumps({"tool_name": "ExitPlanMode", "tool_input": {}, "tool_response": response})


def header(issue=2, stage="Plan", tier=0):
    return f"Issue: #{issue}\nStage: {stage}\nTier: {tier}\n\n# Plan\n..."


class PlanApprovedTest(unittest.TestCase):
    def run_hook(self, raw, gh=None):
        gh = gh or Recorder()
        return plan_approved.run(raw, gh=gh), gh

    def test_plan_stage_adds_approved_and_the_tier(self):
        (code, _), gh = self.run_hook(payload(header(stage="Plan", tier=0)))
        self.assertEqual(code, 0)
        self.assertEqual(len(gh.calls), 1)
        args = gh.calls[0]
        self.assertEqual(args[:3], ["issue", "edit", "2"])
        self.assertIn("grgomez/buCu", args)
        self.assertEqual(args[args.index("--add-label") + 1], "approved")
        self.assertIn("tier:0", args)
        self.assertEqual(
            sorted(args[i + 1] for i, a in enumerate(args) if a == "--remove-label"),
            ["tier:1", "tier:2"],
        )

    def test_spec_stage_adds_spec_approved(self):
        (code, _), gh = self.run_hook(payload(header(issue=7, stage="Spec", tier=2)))
        self.assertEqual(code, 0)
        args = gh.calls[0]
        self.assertEqual(args[2], "7")
        self.assertEqual(args[args.index("--add-label") + 1], "spec-approved")
        self.assertIn("tier:2", args)
        self.assertNotIn("approved", [args[i + 1] for i, a in enumerate(args) if a == "--add-label"])

    def test_plan_without_a_valid_header_is_ignored(self):
        for text in ("# Plan only", "Issue: #2\nStage: Plan", "Issue: #2\nStage: Draft\nTier: 0",
                     "Issue: #2\nStage: Plan\nTier: 3"):
            (code, message), gh = self.run_hook(payload(text))
            self.assertEqual((code, message, gh.calls), (0, "", []), text)

    def test_plan_is_read_from_the_file_when_the_field_is_missing(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "plan.md"
            path.write_text(header(issue=9))
            (code, _), gh = self.run_hook(payload(file_path=str(path)))
        self.assertEqual(code, 0)
        self.assertEqual(gh.calls[0][2], "9")

    def test_no_plan_text_exits_2(self):
        (code, message), gh = self.run_hook(payload())
        self.assertEqual(code, 2)
        self.assertTrue(message)
        self.assertEqual(gh.calls, [])

    def test_unreadable_plan_file_exits_2(self):
        (code, _), _ = self.run_hook(payload(file_path="/nonexistent/plan.md"))
        self.assertEqual(code, 2)

    def test_gh_failure_exits_2_with_a_message(self):
        (code, message), _ = self.run_hook(payload(header()), gh=Recorder(fail=True))
        self.assertEqual(code, 2)
        self.assertIn("#2", message)

    def test_bad_json_is_ignored(self):
        (code, _), _ = self.run_hook("not json")
        self.assertEqual(code, 0)


if __name__ == "__main__":
    unittest.main()
