#!/usr/bin/env python3
"""The scope the CI build jobs obey (tools/release/pr_scope.py, `.github/workflows/pr.yml`).

Both directions are checked on real throwaway git repositories, because the wide answer is the one that matters: a pull request that changes
only documents switches the rebuilds off, and anything a binary could depend on -- a root file the packages stage, a build script, a
directory nobody classified -- leaves them on. An unrunnable comparison (no base commit, an empty diff, a push) is answered by building.
The wiring of the workflow itself is checked by tools/release/test_community_prs.py.
"""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))

import pr_scope  # noqa: E402


def git(repo: Path, *args: str) -> str:
    return subprocess.run(("git", *args), cwd=repo, capture_output=True, text=True, check=True).stdout


@pytest.fixture
def tree(tmp_path: Path) -> Path:
    git(tmp_path, "init", "-q", "--initial-branch=main")
    git(tmp_path, "config", "user.email", "scope@example.invalid")
    git(tmp_path, "config", "user.name", "scope")
    (tmp_path / "engine").mkdir()
    (tmp_path / "docs").mkdir()
    (tmp_path / "engine" / "a.cpp").write_text("1\n")
    (tmp_path / "docs" / "REGULATION.md").write_text("1\n")
    (tmp_path / "README.md").write_text("1\n")
    git(tmp_path, "add", "-A")
    git(tmp_path, "commit", "-qm", "base")
    return tmp_path


def change(repo: Path, files: dict[str, str]) -> None:
    for name, text in files.items():
        f = repo / name
        f.parent.mkdir(parents=True, exist_ok=True)
        f.write_text(text)
    git(repo, "add", "-A")
    git(repo, "commit", "-qm", "the pull request")


def scope_of(repo: Path, monkeypatch, base: str = "HEAD^1", event: str = "pull_request") -> tuple[bool, str]:
    monkeypatch.setattr(pr_scope, "REPO", repo)
    return pr_scope.scope(pr_scope.changed_paths(base), event)


@pytest.mark.parametrize("files", [
    {"docs/new.md": "x\n"},
    {"docs/agents/roles/warden.md": "x\n", "docs/evidence/note.md": "y\n"},
    {"docs/deep/deeper/deepest.md": "x\n"},
])
def test_documents_alone_leave_the_rebuilds_out(tree, monkeypatch, files):
    change(tree, files)
    build, why = scope_of(tree, monkeypatch)
    assert not build and "documents" in why, why


def test_a_deleted_document_leaves_the_rebuilds_out_too(tree, monkeypatch):
    # a pull request that only removes a page is still a documents-only pull request
    git(tree, "rm", "-q", "docs/REGULATION.md")
    git(tree, "commit", "-qm", "a page gone")
    monkeypatch.setattr(pr_scope, "REPO", tree)
    build, why = pr_scope.scope(pr_scope.changed_paths("HEAD^1"), "pull_request")
    assert not build, why


@pytest.mark.parametrize("files,why", [
    ({"README.md": "2\n"}, "a root file the packages stage"),
    ({"LICENSE": "2\n"}, "a root file the packages stage"),
    ({"NOTICE": "2\n"}, "a root file the packages stage"),
    ({"engine/a.cpp": "2\n"}, "an engine source"),
    ({"build.bat": "2\n"}, "a build script"),
    ({"tools/release/build_windows.sh": "2\n"}, "a build script"),
    ({"docs/REGULATION.md": "2\n", "somewhere/new/x.md": "2\n"}, "a directory nobody classified"),
])
def test_anything_a_binary_could_come_from_is_built(tree, monkeypatch, files, why):
    change(tree, files)
    build, reason = scope_of(tree, monkeypatch)
    assert build, (why, reason)
    assert "outside documents" in reason, reason


def test_a_checkout_without_the_base_builds(tree, monkeypatch):
    change(tree, {"docs/x.md": "2\n"})
    build, why = scope_of(tree, monkeypatch, base="HEAD^9")
    assert build and "cannot name" in why, why


def test_an_empty_diff_builds(tree, monkeypatch):
    git(tree, "commit", "-q", "--allow-empty", "-m", "nothing changed")
    build, why = scope_of(tree, monkeypatch)
    assert build and "no file" in why, why


@pytest.mark.parametrize("event", ["push", "merge_group", ""])
def test_a_run_that_is_not_a_pull_request_builds(tree, monkeypatch, event):
    change(tree, {"docs/x.md": "2\n"})
    build, why = scope_of(tree, monkeypatch, event=event)
    assert build and "triggered by" in why, (event, why)


def test_the_step_output_holds_the_answer(tree, tmp_path, monkeypatch, capfd):
    monkeypatch.setattr(pr_scope, "REPO", tree)
    out = tmp_path / "github_output"
    change(tree, {"docs/x.md": "2\n"})
    assert pr_scope.main(["--base", "HEAD^1", "--event", "pull_request", "--github-output", str(out)]) == 0
    assert out.read_text() == "engine=no\n"
    assert "SCOPE: engine build not needed" in capfd.readouterr().out
    assert pr_scope.main(["--base", "HEAD^1", "--event", "push", "--github-output", str(out)]) == 0
    assert out.read_text().splitlines()[-1] == "engine=yes", "a push never opts a build out"


def test_the_reason_names_the_path_that_broke_the_lane(tree, monkeypatch, capfd):
    """the log has to say which file made the run expensive, or the answer cannot be argued with"""
    monkeypatch.setattr(pr_scope, "REPO", tree)
    change(tree, {"docs/x.md": "2\n", "engine/a.cpp": "2\n"})
    out = tree / "github_output"
    assert pr_scope.main(["--base", "HEAD^1", "--event", "pull_request", "--github-output", str(out)]) == 0
    assert "engine/a.cpp" in capfd.readouterr().out and out.read_text() == "engine=yes\n"
