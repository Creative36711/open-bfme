"""Stdlib tests for the all-ref retail history gate."""
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).with_name("retail_history_audit.py")


def run(*args: str, cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(args, cwd=cwd, text=True, capture_output=True, check=False)


def git(path: Path, *args: str) -> None:
    subprocess.run(["git", *args], cwd=path, check=True, capture_output=True, text=True)


def init_repo(path: Path) -> None:
    path.mkdir()
    git(path, "init", "-q", "-b", "main")
    git(path, "config", "user.name", "history-test")
    git(path, "config", "user.email", "history@example.invalid")


def commit_all(path: Path, message: str) -> None:
    git(path, "add", "-A")
    git(path, "commit", "-q", "-m", message)


class RetailHistoryAuditTests(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.tmp_path = Path(self._tmp.name)

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def audit(self, repo: Path, *args: str) -> subprocess.CompletedProcess[str]:
        return run(sys.executable, str(SCRIPT), "--json", *args, cwd=repo)

    def test_clean_history_passes(self) -> None:
        repo = self.tmp_path / "clean"
        init_repo(repo)
        (repo / "README.md").write_text("source only\n", encoding="utf-8")
        commit_all(repo, "clean")

        result = self.audit(repo)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["reachable_retail_paths"], 0)
        self.assertEqual(report["unique_retail_blobs"], 0)

    def test_deleted_retail_and_lfs_pointers_still_fail(self) -> None:
        repo = self.tmp_path / "dirty"
        init_repo(repo)
        (repo / "README.md").write_text("source\n", encoding="utf-8")
        (repo / "BFME2").mkdir()
        (repo / "BFME2" / "game.dat").write_bytes(b"retail bytes" * 100)
        (repo / "RotWK").mkdir()
        (repo / "RotWK" / "data.big").write_text(
            "version https://git-lfs.github.com/spec/v1\n"
            f"oid sha256:{'1' * 64}\n"
            "size 5636073399\n",
            encoding="ascii",
        )
        commit_all(repo, "accidental retail")
        git(repo, "rm", "-qr", "BFME2", "RotWK")
        commit_all(repo, "delete from tip")

        result = self.audit(repo)
        self.assertEqual(result.returncode, 1, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["reachable_retail_paths"], 2)
        self.assertEqual(report["ordinary_blobs"], 1)
        self.assertEqual(report["lfs_pointer_blobs"], 1)
        self.assertEqual(report["lfs_declared_payload_bytes"], 5_636_073_399)
        self.assertEqual(report["ref_tip_retail_paths"]["refs/heads/main"], 0)

    def test_default_all_ref_scan_finds_unmerged_head(self) -> None:
        repo = self.tmp_path / "heads"
        init_repo(repo)
        (repo / "README.md").write_text("source\n", encoding="utf-8")
        commit_all(repo, "base")
        git(repo, "switch", "-q", "-c", "retail-head")
        (repo / "RotWK").mkdir()
        (repo / "RotWK" / "asset.big").write_bytes(b"retail")
        commit_all(repo, "retail on another published head")
        git(repo, "switch", "-q", "main")

        result = self.audit(repo)
        self.assertEqual(result.returncode, 1, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["ordinary_blobs"], 1)
        self.assertEqual(report["ref_tip_retail_paths"]["refs/heads/main"], 0)
        self.assertEqual(report["ref_tip_retail_paths"]["refs/heads/retail-head"], 1)

    def test_shallow_clone_is_not_clean_evidence(self) -> None:
        source = self.tmp_path / "source"
        init_repo(source)
        (source / "README.md").write_text("one\n", encoding="utf-8")
        commit_all(source, "one")
        (source / "README.md").write_text("two\n", encoding="utf-8")
        commit_all(source, "two")
        shallow = self.tmp_path / "shallow"
        subprocess.run(
            ["git", "clone", "-q", "--depth=1", source.as_uri(), str(shallow)],
            check=True,
            capture_output=True,
            text=True,
        )

        result = self.audit(shallow)
        self.assertEqual(result.returncode, 2)
        self.assertIn("repository is shallow", result.stderr)


if __name__ == "__main__":
    unittest.main()
