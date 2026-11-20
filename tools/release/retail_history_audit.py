#!/usr/bin/env python3
"""Fail when BFME2/ or RotWK/ blobs remain reachable from Git refs.

A clean tip is not enough: this inspects every commit reachable from the selected
revision set and accounts for ordinary blobs and Git LFS pointer blobs
separately.  Run from a complete clone before a release and after any history
rewrite.  Exit 0 = clean, 1 = retail objects found, 2 = audit could not prove a
complete result.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

RETAIL_ROOTS = {"bfme2", "rotwk"}
LFS_HEADER = b"version https://git-lfs.github.com/spec/v1\n"
LFS_SIZE = re.compile(br"(?:^|\n)size ([0-9]+)(?:\n|$)")
ZERO_OID = "0" * 40


def git(repo: Path, *args: str, binary: bool = False) -> str | bytes:
    return subprocess.check_output(
        ["git", "-C", str(repo), *args],
        text=not binary,
        stderr=subprocess.PIPE,
    )


def is_retail_path(path: str) -> bool:
    normalized = path.replace("\\", "/").lstrip("./")
    return normalized.split("/", 1)[0].casefold() in RETAIL_ROOTS


def changed_retail_blobs(repo: Path, revisions: list[str]) -> tuple[set[str], set[str]]:
    """Return every retail path and old/new blob named by reachable commit diffs."""
    command = ["log", *revisions, "--raw", "--no-renames", "--no-abbrev", "-z", "--format=format:"]
    tokens = git(repo, *command, binary=True).split(b"\0")
    paths: set[str] = set()
    blobs: set[str] = set()
    i = 0
    while i < len(tokens):
        token = tokens[i]
        if not token.startswith(b":") or i + 1 >= len(tokens):
            i += 1
            continue
        fields = token[1:].decode("ascii", "replace").split()
        path = tokens[i + 1].decode("utf-8", "surrogateescape")
        i += 2
        if len(fields) < 5 or not is_retail_path(path):
            continue
        paths.add(path)
        # :oldmode newmode oldoid newoid status. Include deleted and replaced
        # versions: either remains reachable through the commit that held it.
        for oid in fields[2:4]:
            if oid != ZERO_OID and set(oid) != {"0"}:
                blobs.add(oid)
    return paths, blobs


def object_metadata(repo: Path, oids: set[str]) -> dict[str, tuple[str, int]]:
    if not oids:
        return {}
    ordered = sorted(oids)
    result = subprocess.run(
        ["git", "-C", str(repo), "cat-file", "--batch-check=%(objectname) %(objecttype) %(objectsize)"],
        input="".join(f"{oid}\n" for oid in ordered),
        text=True,
        capture_output=True,
        check=True,
    )
    out: dict[str, tuple[str, int]] = {}
    for line in result.stdout.splitlines():
        fields = line.split()
        if len(fields) == 3:
            out[fields[0]] = (fields[1], int(fields[2]))
    return out


def object_contents(repo: Path, oids: list[str]) -> dict[str, bytes]:
    if not oids:
        return {}
    process = subprocess.Popen(
        ["git", "-C", str(repo), "cat-file", "--batch"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    assert process.stdin is not None and process.stdout is not None
    process.stdin.write("".join(f"{oid}\n" for oid in oids).encode("ascii"))
    process.stdin.close()
    out: dict[str, bytes] = {}
    for requested in oids:
        header = process.stdout.readline().decode("ascii", "replace").strip().split()
        if len(header) != 3:
            process.kill()
            raise RuntimeError(f"git cat-file returned a malformed header for {requested}: {header}")
        oid, kind, size_text = header
        size = int(size_text)
        data = process.stdout.read(size)
        terminator = process.stdout.read(1)
        if kind != "blob" or len(data) != size or terminator != b"\n":
            process.kill()
            raise RuntimeError(f"git cat-file could not read blob {requested}")
        out[oid] = data
    stderr = process.stderr.read().decode("utf-8", "replace") if process.stderr else ""
    rc = process.wait()
    if rc:
        raise RuntimeError(f"git cat-file failed ({rc}): {stderr.strip()}")
    return out


def ref_tip_counts(repo: Path) -> dict[str, int]:
    text = git(repo, "for-each-ref", "--format=%(refname)", "refs/heads", "refs/remotes", "refs/tags")
    counts: dict[str, int] = {}
    for ref in sorted(set(text.splitlines())):
        if ref.endswith("/HEAD"):
            continue
        names = git(repo, "ls-tree", "-r", "-z", "--name-only", ref, binary=True)
        count = sum(is_retail_path(raw.decode("utf-8", "surrogateescape")) for raw in names.split(b"\0") if raw)
        counts[ref] = count
    return counts


def audit(repo: Path, revisions: list[str]) -> dict:
    paths, candidate_oids = changed_retail_blobs(repo, revisions)
    metadata = object_metadata(repo, candidate_oids)
    blob_meta = {oid: size for oid, (kind, size) in metadata.items() if kind == "blob"}
    small = [oid for oid, size in sorted(blob_meta.items()) if size <= 1024]
    contents = object_contents(repo, small)

    lfs: dict[str, int] = {}
    for oid, data in contents.items():
        if not data.startswith(LFS_HEADER):
            continue
        match = LFS_SIZE.search(data)
        if match:
            lfs[oid] = int(match.group(1))
    ordinary = {oid: size for oid, size in blob_meta.items() if oid not in lfs}

    return {
        "schema": "openbfme.retail-history-audit.v1",
        "repository": str(repo.resolve()),
        "revisions": revisions,
        "reachable_retail_paths": len(paths),
        "unique_retail_blobs": len(blob_meta),
        "ordinary_blobs": len(ordinary),
        "ordinary_git_bytes": sum(ordinary.values()),
        "lfs_pointer_blobs": len(lfs),
        "lfs_pointer_git_bytes": sum(blob_meta[oid] for oid in lfs),
        "lfs_declared_payload_bytes": sum(lfs.values()),
        "ref_tip_retail_paths": ref_tip_counts(repo) if revisions == ["--all"] else {},
        "sample_paths": sorted(paths)[:20],
    }


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path.cwd(), help="Git repository (default: current directory)")
    parser.add_argument(
        "--revision",
        action="append",
        dest="revisions",
        help="revision to inspect; repeat for several (default: every local ref with --all)",
    )
    parser.add_argument("--json", action="store_true", help="print the full machine-readable report")
    parser.add_argument(
        "--allow-shallow",
        action="store_true",
        help="allow an incomplete diagnostic; never use this to claim that history is clean",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv or sys.argv[1:])
    repo = args.repo.resolve()
    try:
        inside = git(repo, "rev-parse", "--is-inside-work-tree").strip() == "true"
        bare = git(repo, "rev-parse", "--is-bare-repository").strip() == "true"
        shallow = git(repo, "rev-parse", "--is-shallow-repository").strip() == "true"
    except (subprocess.CalledProcessError, FileNotFoundError) as error:
        print(f"retail-history-audit: cannot open Git repository {repo}: {error}", file=sys.stderr)
        return 2
    if not inside and not bare:
        print(f"retail-history-audit: {repo} is not a Git repository", file=sys.stderr)
        return 2
    if shallow and not args.allow_shallow:
        print("retail-history-audit: repository is shallow; fetch complete history or use a full mirror", file=sys.stderr)
        return 2

    revisions = args.revisions or ["--all"]
    try:
        report = audit(repo, revisions)
    except (subprocess.CalledProcessError, RuntimeError, ValueError) as error:
        print(f"retail-history-audit: audit failed: {error}", file=sys.stderr)
        return 2

    dirty = report["unique_retail_blobs"] > 0
    if args.json:
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        verdict = "FAIL" if dirty else "OK"
        print(
            f"retail-history-audit: {verdict}: {report['reachable_retail_paths']} paths, "
            f"{report['ordinary_blobs']} ordinary blobs/{report['ordinary_git_bytes']} bytes, "
            f"{report['lfs_pointer_blobs']} LFS pointers/{report['lfs_declared_payload_bytes']} declared bytes"
        )
        for ref, count in report["ref_tip_retail_paths"].items():
            if count:
                print(f"  {ref}: {count} retail paths at tip")
    return 1 if dirty else 0


if __name__ == "__main__":
    raise SystemExit(main())
