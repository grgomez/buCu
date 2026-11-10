"""Tests for tools/sdd_check.py."""

import importlib.util
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("sdd_check", ROOT / "tools" / "sdd_check.py")
sdd_check = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(sdd_check)

SPEC_PART = "<!-- SPEC:START -->\n# Spec\n### US1: a\n- [ ] US1-AC1: x\n<!-- SPEC:END -->\n"
PLAN_PART = "<!-- PLAN:START -->\n# Plan\n<!-- PLAN:END -->\n"
TASKS_PART = "<!-- TASKS:START -->\n# Tasks\n- [ ] T001 x\n<!-- TASKS:END -->\n"


def body(original="Goal.\n", sep="---\n", spec=SPEC_PART, plan=PLAN_PART, tasks=TASKS_PART):
    return f"{original}\n{sep}{spec}{plan}{tasks}"


def labels(tier, *extra):
    return [f"tier:{tier}", "approved", *extra]


class PartsTest(unittest.TestCase):
    def check(self, text, names):
        return sdd_check.check_parts(text, names)

    def test_tier1_body_passes(self):
        self.assertEqual(self.check(body(), labels(1)), [])

    def test_tier0_without_spec_passes(self):
        self.assertEqual(self.check(body(spec=""), labels(0)), [])

    def test_tier2_needs_spec_approved(self):
        self.assertEqual(self.check(body(), labels(2, "spec-approved")), [])
        self.assertTrue(any("spec-approved" in f for f in self.check(body(), labels(2))))

    def test_label_failures(self):
        self.assertTrue(any("tier" in f for f in self.check(body(), ["approved"])))
        self.assertTrue(any("tier" in f for f in self.check(body(), ["approved", "tier:0", "tier:1"])))
        self.assertTrue(any("approved" in f for f in self.check(body(), ["tier:1"])))

    def test_tier0_must_not_have_spec(self):
        self.assertTrue(any("Spec" in f for f in self.check(body(), labels(0))))

    def test_tier1_needs_spec(self):
        self.assertTrue(any("Spec" in f for f in self.check(body(spec=""), labels(1))))

    def test_plan_and_tasks_required(self):
        self.assertTrue(any("PLAN" in f for f in self.check(body(plan=""), labels(1))))
        self.assertTrue(any("TASKS" in f for f in self.check(body(tasks=""), labels(1))))

    def test_separator_required(self):
        self.assertTrue(any("---" in f for f in self.check(body(sep=""), labels(1))))

    def test_marker_must_be_alone_on_its_line(self):
        text = body().replace("<!-- PLAN:END -->", "text <!-- PLAN:END -->")
        self.assertTrue(any("PLAN:END" in f for f in self.check(text, labels(1))))

    def test_marker_twice(self):
        text = body() + PLAN_PART
        self.assertTrue(any("PLAN:START" in f for f in self.check(text, labels(1))))

    def test_order(self):
        text = body(plan=TASKS_PART, tasks=PLAN_PART)
        self.assertTrue(any("order" in f for f in self.check(text, labels(1))))

    def test_empty_part(self):
        text = body(plan="<!-- PLAN:START -->\n<!-- PLAN:END -->\n")
        self.assertTrue(any("empty" in f for f in self.check(text, labels(1))))


if __name__ == "__main__":
    unittest.main()
