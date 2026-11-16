"""Tests for tools/sdd_check.py."""

import importlib.util
import io
import json
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("sdd_check", ROOT / "tools" / "sdd_check.py")
sdd_check = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(sdd_check)

SPEC_PART = "<!-- SPEC:START -->\n# Spec\n### US1: a\n- [ ] US1-AC1: x Evidence: test\n<!-- SPEC:END -->\n"
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


class DeclaredEvidenceTest(unittest.TestCase):
    def failures(self, criterion, tier=1):
        spec = f"<!-- SPEC:START -->\n# Spec\n### US1: a\n{criterion}\n<!-- SPEC:END -->\n"
        return sdd_check.check_declared(body(spec=spec), tier)

    def test_each_kind_passes(self):
        for kind in ("test", "property", "fuzz", "proof", "check", "manual", "after merge"):
            self.assertEqual(self.failures(f"- [ ] US1-AC1: x. Evidence: {kind}"), [], kind)

    def test_completed_form_passes(self):
        self.assertEqual(self.failures("- [x] US1-AC1: x Evidence: check: T004"), [])

    def test_missing_or_unknown_kind_fails(self):
        for criterion in ("- [ ] US1-AC1: x", "- [ ] US1-AC1: x Evidence: guess",
                          "- [ ] US1-AC1: x Evidence:", "- [ ] US1-AC1: x Evidence: testing"):
            failures = self.failures(criterion)
            self.assertEqual(len(failures), 1, criterion)
            self.assertIn("US1-AC1", failures[0])

    def test_tier0_scope_bullets_need_a_kind(self):
        original = "Goal.\n\n## Scope\n\n- first. Evidence: check\n- second\n"
        tasks = "<!-- TASKS:START -->\n# Tasks\n- [ ] T001 x\n<!-- TASKS:END -->\n"
        failures = sdd_check.check_declared(body(original=original, spec="", tasks=tasks), 0)
        self.assertEqual(len(failures), 1)
        self.assertIn("second", failures[0])

    def test_issue_command_reports_it(self):
        spec = "<!-- SPEC:START -->\n# Spec\n### US1: a\n- [ ] US1-AC1: x\n<!-- SPEC:END -->\n"
        gh = CommandLineTest().fake_gh(body(spec=spec), labels(1))
        err = io.StringIO()
        self.assertEqual(sdd_check.main(["issue", "7"], gh=gh, err=err), 1)
        self.assertIn("issue #7: ", err.getvalue())
        self.assertIn("US1-AC1", err.getvalue())


class CheckboxTest(unittest.TestCase):
    def tier0(self, original):
        tasks = "<!-- TASKS:START -->\n# Tasks\n- [ ] T001 x\n<!-- TASKS:END -->\n"
        return body(original=original, spec="", tasks=tasks)

    def test_tier0_bullet_without_box_fails(self):
        text = self.tier0("Goal.\n\n## Scope\n\n- [ ] first. Evidence: check\n- second. Evidence: check\n")
        failures = sdd_check.check_checkboxes(text, 0)
        self.assertEqual(len(failures), 1)
        self.assertIn("criterion has no checkbox: - second", failures[0])

    def test_tier0_boxed_bullets_pass(self):
        text = self.tier0("Goal.\n\n## Scope\n\n- [ ] first. Evidence: check\n- [x] second. Evidence: check\n")
        self.assertEqual(sdd_check.check_checkboxes(text, 0), [])

    def test_issue_command_reports_a_tier0_bullet_without_box(self):
        text = self.tier0("Goal.\n\n## Scope\n\n- first. Evidence: check\n")
        gh = CommandLineTest().fake_gh(text, labels(0))
        err = io.StringIO()
        self.assertEqual(sdd_check.main(["issue", "7"], gh=gh, err=err), 1)
        self.assertIn("issue #7: criterion has no checkbox: - first", err.getvalue())

    def test_pr_mode_reports_a_missing_box_once(self):
        text = self.tier0("Goal.\n\n## Scope\n\n- first. Evidence: check\n")
        gh = CommandLineTest().fake_gh(text, labels(0))
        err = io.StringIO()
        sdd_check.main(["pr", "9"], gh=gh, err=err)
        self.assertEqual(err.getvalue().count("criterion has no checkbox"), 1)

    def test_pr_mode_reports_a_tier1_spec_line_without_box_once(self):
        text = done_body(criterion="- US1-AC1: x Evidence: test: a_test.py: case")
        gh = CommandLineTest().fake_gh(text, labels(1))
        err = io.StringIO()
        self.assertEqual(sdd_check.main(["pr", "9"], gh=gh, err=err), 1)
        self.assertEqual(err.getvalue().count("criterion has no checkbox"), 1)

    def spec_failures(self, criterion):
        spec = f"<!-- SPEC:START -->\n# Spec\n### US1: a\n{criterion}\n<!-- SPEC:END -->\n"
        return sdd_check.check_checkboxes(body(spec=spec), 1)

    def test_spec_criterion_without_box_fails(self):
        for line in ("US1-AC1: x Evidence: test", "- US1-AC1: x Evidence: test"):
            failures = self.spec_failures(line)
            self.assertEqual(len(failures), 1, line)
            self.assertIn("criterion has no checkbox", failures[0])
            self.assertIn("US1-AC1", failures[0])

    def test_indented_boxed_criterion_is_not_boxless_and_is_counted(self):
        self.assertEqual(self.spec_failures("  - [ ] US1-AC1: x Evidence: test"), [])
        criteria = "\n".join(f"  - [ ] US1-AC{n}: x Evidence: test" for n in range(1, 14))
        spec = f"<!-- SPEC:START -->\n# Spec\n### US1: a\n{criteria}\n<!-- SPEC:END -->\n"
        self.assertTrue(any("13 criteria" in f for f in sdd_check.check_size(body(spec=spec))))

    def test_spec_boxed_criterion_and_prose_pass(self):
        self.assertEqual(self.spec_failures("- [ ] US1-AC1: x Evidence: test"), [])
        self.assertEqual(self.spec_failures("- [x] US1-AC1: x Evidence: test: a: b"), [])
        self.assertEqual(self.spec_failures("See US1-AC1: for the rule."), [])


def done_body(task="- [x] T001 x", criterion="- [x] US1-AC1: x Evidence: test: a_test.py: case"):
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
        for evidence in ("Evidence: manual: check 1 (owner)", "Evidence: after merge (owner)"):
            text = done_body(criterion=f"- [ ] US1-AC1: x {evidence}")
            self.assertEqual(self.failures(text), [])

    def test_unchecked_criterion_with_test_evidence(self):
        text = done_body(criterion="- [ ] US1-AC1: x Evidence: test: a_test.py: case")
        self.assertTrue(any("US1-AC1" in f for f in self.failures(text)))

    def test_ticked_kinds_with_a_location_pass(self):
        for evidence in ("test: a_test.py: case", "property: a_test.cpp: round trip",
                         "fuzz: fuzz/x.cpp: corpus run", "proof: proofs/x.v: lemma",
                         "check: T004", "check: T004, T005", "manual: check 1 (owner)",
                         "after merge (owner)"):
            text = done_body(criterion=f"- [x] US1-AC1: x Evidence: {evidence}")
            self.assertEqual(self.failures(text), [], evidence)

    def test_ticked_kind_without_its_location_fails(self):
        for evidence in ("test", "test: a_test.py", "property: a_test.cpp:", "check",
                         "check: first task", "check: T4", "manual", "manual: check 1",
                         "after merge", "guess: x: y"):
            text = done_body(criterion=f"- [x] US1-AC1: x Evidence: {evidence}")
            failures = self.failures(text)
            self.assertEqual(len(failures), 1, evidence)
            self.assertIn("US1-AC1", failures[0])

    def test_unticked_kind_must_be_manual_or_after_merge_in_full(self):
        for evidence in ("manual", "manual, check 1 (owner)", "after merge", "check: T004"):
            text = done_body(criterion=f"- [ ] US1-AC1: x Evidence: {evidence}")
            self.assertEqual(len(self.failures(text)), 1, evidence)

    def test_tier0_scope_bullets_are_the_criteria(self):
        original = "Goal.\n\n## Scope\n\n- [x] first. Evidence: check: T001\n- second\n"
        tasks = "<!-- TASKS:START -->\n# Tasks\n- [x] T001 x\n<!-- TASKS:END -->\n"
        text = body(original=original, spec="", tasks=tasks)
        failures = self.failures(text, tier=0)
        self.assertEqual(len(failures), 1)
        self.assertIn("second", failures[0])


class LivingDocumentsTest(unittest.TestCase):
    PR = "Closes #1\n\n## Living documents\n\n{}\n\n## Checks\n- [x] done\n"

    def failures(self, changed, section, plan_text=""):
        plan = f"<!-- PLAN:START -->\n# Plan\n{plan_text}\n<!-- PLAN:END -->\n"
        return sdd_check.check_living_documents(
            changed, self.PR.format(section), body(plan=plan))

    def test_which_paths_are_living_documents(self):
        living = sdd_check.is_living_document
        for path in ("CLAUDE.md", "README.md", "docs/adr/0004-x.md", "docs/product.md",
                     ".claude/skills/pr/SKILL.md", ".claude/agents/builder.md"):
            self.assertTrue(living(path), path)
        for path in ("tools/sdd_check.py", ".github/pull_request_template.md", "src/main.cpp",
                     ".claude/hooks/pre_tool.py", "CHANGELOG.md"):
            self.assertFalse(living(path), path)

    def test_no_living_document_changed(self):
        self.assertEqual(self.failures(["tools/x.py"], "None."), [])

    def test_changed_document_listed_in_both(self):
        self.assertEqual(self.failures(["docs/product.md"], "- `docs/product.md`: x",
                                       "- `docs/product.md`: new rule"), [])

    def test_missing_from_pr_section(self):
        failures = self.failures(["docs/product.md"], "None.", "docs/product.md")
        self.assertEqual(len(failures), 1)
        self.assertIn("Living documents", failures[0])

    def test_missing_from_plan(self):
        failures = self.failures(["docs/product.md"], "docs/product.md", "nothing")
        self.assertEqual(len(failures), 1)
        self.assertIn("Plan", failures[0])

    def test_section_required(self):
        failures = sdd_check.check_living_documents([], "Closes #1\n", body())
        self.assertTrue(any("Living documents" in f for f in failures))

    def test_section_ends_at_the_next_heading(self):
        pr = "## Living documents\nNone.\n## Checks\ndocs/product.md\n"
        failures = sdd_check.check_living_documents(["docs/product.md"], pr, body())
        self.assertTrue(any("Living documents" in f for f in failures))


class CommandLineTest(unittest.TestCase):
    PR_OK = "Closes #7\n\n## Living documents\nNone.\n"

    def fake_gh(self, issue_body, names, pr_body=PR_OK, files=("tools/x.py",), fail=False):
        calls = []

        def gh(args):
            calls.append(args)
            if fail:
                raise subprocess.CalledProcessError(1, "gh")
            if args[:2] == ["issue", "view"]:
                return json.dumps({"body": issue_body, "labels": [{"name": n} for n in names]})
            if args[:2] == ["pr", "view"]:
                return json.dumps({"body": pr_body, "files": [{"path": p} for p in files]})
            raise AssertionError(args)

        gh.calls = calls
        return gh

    def run_main(self, argv, gh):
        err = io.StringIO()
        return sdd_check.main(argv, gh=gh, err=err), err.getvalue()

    def test_issue_passes(self):
        code, err = self.run_main(["issue", "7"], self.fake_gh(body(), labels(1)))
        self.assertEqual((code, err), (0, ""))

    def test_issue_reports_one_line_per_failure(self):
        code, err = self.run_main(["issue", "7"], self.fake_gh(body(spec=""), ["tier:1"]))
        self.assertEqual(code, 1)
        self.assertEqual(len(err.splitlines()), 2)
        self.assertTrue(all(line.startswith("issue #7: ") for line in err.splitlines()))

    def test_pr_passes(self):
        gh = self.fake_gh(done_body(), labels(1))
        self.assertEqual(self.run_main(["pr", "9"], gh), (0, ""))
        self.assertEqual(gh.calls[-1][:3], ["issue", "view", "7"])

    def test_pr_without_closes(self):
        code, err = self.run_main(["pr", "9"], self.fake_gh(done_body(), labels(1), pr_body="x"))
        self.assertEqual(code, 1)
        self.assertIn("Closes #", err)

    def test_pr_reports_done_and_living_document_failures(self):
        gh = self.fake_gh(body(), labels(1), files=("docs/product.md",))
        code, err = self.run_main(["pr", "9"], gh)
        self.assertEqual(code, 1)
        self.assertIn("T001", err)
        self.assertIn("US1-AC1", err)
        self.assertIn("docs/product.md", err)

    def test_usage_errors_exit_2(self):
        gh = self.fake_gh(body(), labels(1))
        for argv in ([], ["issue"], ["issue", "x"], ["other", "1"], ["issue", "1", "2"]):
            code, err = self.run_main(argv, gh)
            self.assertEqual(code, 2, argv)
            self.assertIn("usage", err)

    def test_gh_failure_exits_2(self):
        code, err = self.run_main(["issue", "7"], self.fake_gh("", [], fail=True))
        self.assertEqual(code, 2)
        self.assertIn("gh", err)

    def test_bad_json_exits_2(self):
        code, _ = self.run_main(["issue", "7"], lambda args: "not json")
        self.assertEqual(code, 2)


class PrTemplateTest(unittest.TestCase):
    def filled(self):
        text = (ROOT / ".github" / "pull_request_template.md").read_text(encoding="utf-8")
        return text.replace("#<n>", "#34")

    def test_filled_template_passes_the_body_checks(self):
        text = self.filled()
        self.assertEqual(sdd_check.closing_issue(text), 34)
        self.assertEqual(sdd_check.check_living_documents([], text, body()), [])

    def test_template_has_the_sections(self):
        text = self.filled()
        for heading in ("## Summary", "## Living documents", "## Checks"):
            self.assertIn(heading, text)

    def test_unfilled_template_fails_the_closes_check(self):
        text = (ROOT / ".github" / "pull_request_template.md").read_text(encoding="utf-8")
        self.assertIsNone(sdd_check.closing_issue(text))

    def test_no_buku_templates_remain(self):
        for name in ("ISSUE_TEMPLATE.md", "PULL_REQUEST_TEMPLATE.md"):
            self.assertFalse((ROOT / ".github" / name).exists(), name)


if __name__ == "__main__":
    unittest.main()
