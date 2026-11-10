"""Tests for the shell hooks in .githooks/, run in throwaway git repositories."""

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HOOKS = ROOT / ".githooks"


def git(repo, *args, **kwargs):
    return subprocess.run(
        ["git", "-C", str(repo), *args], capture_output=True, text=True, check=True, **kwargs
    )


class Sandbox(unittest.TestCase):
    """A throwaway repository, and a bin directory placed first on PATH."""

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmp = Path(self._tmp.name)
        self.repo = self.tmp / "repo"
        self.repo.mkdir()
        git(self.repo, "init", "-q", "-b", "master")
        git(self.repo, "config", "user.email", "t@example.com")
        git(self.repo, "config", "user.name", "T")
        self.bin = self.tmp / "bin"
        self.bin.mkdir()

    def env(self, only_bin=False):
        env = dict(os.environ)
        env["PATH"] = str(self.bin) + ("" if only_bin else os.pathsep + env["PATH"])
        return env

    def run_hook(self, name, *args, stdin="", only_bin=False):
        return subprocess.run(
            [shutil.which("sh"), str(HOOKS / name), *args],
            cwd=self.repo, input=stdin, capture_output=True, text=True,
            env=self.env(only_bin), check=False,
        )


class PreCommitTest(Sandbox):
    def fake_clang_format(self):
        """Fails on any input that contains BAD, and logs the filename it was given."""
        script = self.bin / "clang-format"
        script.write_text(
            '#!/bin/sh\n'
            'for a in "$@"; do case "$a" in --assume-filename=*) echo "${a#*=}" >> "$0.log";; esac; done\n'
            '! grep -q BAD\n'
        )
        script.chmod(0o755)

    def stage(self, name, text):
        path = self.repo / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        git(self.repo, "add", name)

    def test_passes_when_no_cpp_is_staged_even_without_clang_format(self):
        self.stage("docs/a.md", "BAD\n")
        for name in ("git", "grep", "dirname", "cat"):
            os.symlink(shutil.which(name), self.bin / name)
        self.assertEqual(self.run_hook("pre-commit", only_bin=True).returncode, 0)

    def test_fails_when_cpp_is_staged_and_clang_format_is_missing(self):
        self.stage("src/a.cpp", "int main() {}\n")
        for name in ("git", "grep", "dirname", "cat"):
            os.symlink(shutil.which(name), self.bin / name)
        result = self.run_hook("pre-commit", only_bin=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("clang-format", result.stderr)

    def test_passes_for_formatted_cpp(self):
        self.fake_clang_format()
        self.stage("src/a.cpp", "int main() {}\n")
        self.stage("include/a.hpp", "#pragma once\n")
        self.assertEqual(self.run_hook("pre-commit").returncode, 0)

    def test_fails_naming_the_unformatted_file(self):
        self.fake_clang_format()
        self.stage("src/ok.cpp", "int a;\n")
        self.stage("src/bad.cpp", "BAD\n")
        result = self.run_hook("pre-commit")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("src/bad.cpp", result.stderr)

    def test_checks_the_staged_blob_not_the_working_tree(self):
        self.fake_clang_format()
        self.stage("src/a.cpp", "int a;\n")
        (self.repo / "src/a.cpp").write_text("BAD\n")
        self.assertEqual(self.run_hook("pre-commit").returncode, 0)

    def test_checks_a_renamed_unformatted_file(self):
        self.fake_clang_format()
        body = "".join(f"int v{i};\n" for i in range(20))
        self.stage("src/old.cpp", body)
        git(self.repo, "commit", "-q", "--no-verify", "-m", "x")
        git(self.repo, "mv", "src/old.cpp", "src/new.cpp")
        (self.repo / "src/new.cpp").write_text(body + "BAD\n")
        git(self.repo, "add", "src/new.cpp")
        result = self.run_hook("pre-commit")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("src/new.cpp", result.stderr)

    def test_ignores_non_cpp_and_deleted_files(self):
        self.fake_clang_format()
        self.stage("README.md", "BAD\n")
        self.stage("src/gone.cpp", "int a;\n")
        git(self.repo, "commit", "-q", "--no-verify", "-m", "x")
        git(self.repo, "rm", "-q", "src/gone.cpp")
        self.assertEqual(self.run_hook("pre-commit").returncode, 0)


ZERO = "0" * 40
SHA = "a" * 40


class PrePushTest(Sandbox):
    def push(self, remote_ref, local_sha=SHA):
        line = f"refs/heads/local {local_sha} {remote_ref} {ZERO}\n"
        return self.run_hook("pre-push", "origin", "url", stdin=line)

    def test_allows_issue_branches(self):
        for name in ("feat/issue-2-sdd-hooks", "fix/issue-17-a", "feat/issue-100-x1-y2"):
            self.assertEqual(self.push(f"refs/heads/{name}").returncode, 0, name)

    def test_rejects_master(self):
        result = self.push("refs/heads/master")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("master", result.stderr)

    def test_rejects_master_even_from_another_local_branch_or_deletion(self):
        self.assertNotEqual(self.push("refs/heads/master", local_sha=ZERO).returncode, 0)

    def test_rejects_other_branch_names(self):
        for name in ("main", "feature/x", "feat/issue-2", "feat/issue-x-a", "feat/issue-2-Upper",
                     "docs/issue-2-a", "feat/issue-2-a/b"):
            result = self.push(f"refs/heads/{name}")
            self.assertNotEqual(result.returncode, 0, name)
            self.assertIn("feat|fix/issue-<n>-<slug>", result.stderr)

    def test_allows_deleting_a_non_master_branch(self):
        self.assertEqual(self.push("refs/heads/old-name", local_sha=ZERO).returncode, 0)

    def test_checks_every_ref_in_one_push(self):
        stdin = (f"refs/heads/a {SHA} refs/heads/feat/issue-2-a {ZERO}\n"
                 f"refs/heads/b {SHA} refs/heads/bad {ZERO}\n")
        self.assertNotEqual(self.run_hook("pre-push", "origin", "url", stdin=stdin).returncode, 0)

    def test_ignores_tags_and_empty_input(self):
        self.assertEqual(self.push("refs/tags/v1").returncode, 0)
        self.assertEqual(self.run_hook("pre-push", "origin", "url").returncode, 0)


if __name__ == "__main__":
    unittest.main()
