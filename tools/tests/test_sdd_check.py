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


class SizeTest(unittest.TestCase):
    def stories(self, n):
        return "".join(f"### US{i}: s\n" for i in range(1, n + 1))

    def criteria(self, n):
        return "".join(f"- [ ] US1-AC{i}: c\n" for i in range(1, n + 1))

    def tasks(self, n):
        return "".join(f"- [ ] T{i:03d} t\n" for i in range(1, n + 1))

    def wrap(self, name, text):
        return f"<!-- {name}:START -->\n# {name}\n{text}<!-- {name}:END -->\n"

    def test_limits_pass(self):
        text = body(spec=self.wrap("SPEC", self.stories(3) + self.criteria(12)),
                    tasks=self.wrap("TASKS", self.tasks(12)))
        self.assertEqual(sdd_check.check_size(text), [])

    def test_four_stories(self):
        text = body(spec=self.wrap("SPEC", self.stories(4)))
        self.assertTrue(any("user stories" in f for f in sdd_check.check_size(text)))

    def test_thirteen_criteria(self):
        text = body(spec=self.wrap("SPEC", self.stories(1) + self.criteria(13)))
        self.assertTrue(any("criteria" in f for f in sdd_check.check_size(text)))

    def test_thirteen_tasks(self):
        text = body(tasks=self.wrap("TASKS", self.tasks(13)))
        self.assertTrue(any("tasks" in f for f in sdd_check.check_size(text)))

    def test_tier0_counts_scope_bullets_and_task_stories(self):
        scope = "## Scope\n\n" + "".join(f"- item {i}\n" for i in range(13)) + "\n## Out of scope\n"
        text = body(original="Goal.\n\n" + scope, spec="",
                    tasks=self.wrap("TASKS", self.stories(4).replace("###", "##")))
        failures = sdd_check.check_size(text)
        self.assertTrue(any("criteria" in f for f in failures))
        self.assertTrue(any("user stories" in f for f in failures))

    def test_open_marker_in_a_part(self):
        text = body(plan=self.wrap("PLAN", "[NEEDS CLARIFICATION: which?]\n"))
        self.assertTrue(any("NEEDS CLARIFICATION" in f for f in sdd_check.check_size(text)))

    def test_marker_without_colon_is_prose(self):
        text = body(plan=self.wrap("PLAN", "leftover `[NEEDS CLARIFICATION` markers\n"))
        self.assertEqual(sdd_check.check_size(text), [])

    def test_marker_in_the_original_text_is_ignored(self):
        text = body(original="[NEEDS CLARIFICATION: owner note]\n")
        self.assertEqual(sdd_check.check_size(text), [])


def done_body(task="- [x] T001 x", criterion="- [x] US1-AC1: x Evidence: a_test.py: case"):
    spec = f"<!-- SPEC:START -->\n# Spec\n### US1: a\n{criterion}\n<!-- SPEC:END -->\n"
    tasks = f"<!-- TASKS:START -->\n# Tasks\n{task}\n<!-- TASKS:END -->\n"
    return body(spec=spec, tasks=tasks)


class DoneTest(unittest.TestCase):
    def failures(self, text, tier=1):
        return sdd_check.check_done(text, [f"tier:{tier}", "approved"])

    def test_closes_line(self):
        self.assertEqual(sdd_check.closing_issue("Text\n\nCloses #34\n"), 34)
        self.assertIsNone(sdd_check.closing_issue("Closes #<n>"))
        self.assertIsNone(sdd_check.closing_issue("Fixes #3"))

    def test_done_body_passes(self):
        self.assertEqual(self.failures(done_body()), [])

    def test_unticked_task(self):
        self.assertTrue(any("T001" in f for f in self.failures(done_body(task="- [ ] T001 x"))))

    def test_checked_criterion_needs_evidence(self):
        text = done_body(criterion="- [x] US1-AC1: x")
        self.assertTrue(any("US1-AC1" in f for f in self.failures(text)))

    def test_unchecked_criterion_with_manual_or_after_merge(self):
        for evidence in ("Evidence: manual, check 1 (owner)", "Evidence: after merge (owner)"):
            text = done_body(criterion=f"- [ ] US1-AC1: x {evidence}")
            self.assertEqual(self.failures(text), [])

    def test_unchecked_criterion_with_test_evidence(self):
        text = done_body(criterion="- [ ] US1-AC1: x Evidence: a_test.py: case")
        self.assertTrue(any("US1-AC1" in f for f in self.failures(text)))

    def test_tier0_scope_bullets_are_the_criteria(self):
        original = "Goal.\n\n## Scope\n\n- [x] first. Evidence: T001 Check\n- second\n"
        tasks = "<!-- TASKS:START -->\n# Tasks\n- [x] T001 x\n<!-- TASKS:END -->\n"
        text = body(original=original, spec="", tasks=tasks)
        failures = self.failures(text, tier=0)
        self.assertEqual(len(failures), 1)
        self.assertIn("second", failures[0])


if __name__ == "__main__":
    unittest.main()
