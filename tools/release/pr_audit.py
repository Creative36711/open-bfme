#!/usr/bin/env python3
"""Lane GHPR-1: the public pull request check's audit of the tree a pull request would make (.github/workflows/pr.yml): the release sync's
privacy audit (autorelease.audit_tree: the public top level, no private folders, nothing a .gitignore rule ignores, the commit gate's
rules on every file, containers, binaries) with its BUILT-IN rules only (a public runner has no private denylist: this is not a
publishing check, the release audits again with the operator's list), plus the .github policy (public_workflows.py).

Fails closed: a tree without the public tree's core entries (an empty or truncated checkout) is refused, as is a failing git call.

    python3 tools/release/pr_audit.py [--git-dir DIR] [--tree TREE] [--base REV]

--base REV (the pull request's own base, e.g. origin/main) attributes every finding: a finding the base tree does not have is the audited
tree's own, a finding the base tree has too was inherited and belongs to whoever committed it. The audit itself stays red - the exit code
does not depend on --base - attribution only says whose it is, so a role is never pushed into patching someone else's file to get a green
tick, and never told a finding is about its own work when it is not (tools/script-cat/preflight.mjs uses this before a push).
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import autorelease  # noqa: E402
import public_workflows  # noqa: E402

# what every public tree holds: a checkout without one of these is not a whole checkout of the public repository (empty, shallow-broken or truncated)
CORE = ("README.md", "LICENSE", "CONTRIBUTING.md", ".github", "docs", "engine", "godot", "launcher", "tools")

# a finding names the file or top level it is about; the path is not always in the same place in the sentence, so every
# token that is really a path of the tree is a candidate and the blame uses the first one ("developer-machine path in <path>",
# "not a public top-level entry: <name>", "<path>: <detail>")
FINDING_TOKEN = re.compile(r"[A-Za-z0-9._-]+(?:/[A-Za-z0-9._-]+)*")


def finding_paths(git_dir: str | None, tree: str, problem: str) -> list[str]:
    """the paths a finding talks about, in the order they appear"""
    found = []
    for token in FINDING_TOKEN.findall(problem):
        if token in found or "/" not in token and "." not in token and len(token) < 4:
            continue
        r = subprocess.run(["git"] + (["-C", git_dir] if git_dir else []) + ["ls-tree", "--name-only", tree, "--", token],
                           capture_output=True, text=True)
        if r.returncode:
            raise subprocess.CalledProcessError(r.returncode, r.args, r.stdout, r.stderr)
        if r.stdout.strip():
            found.append(token)
    return found


def blame(git_dir: str | None, tree: str, problem: str) -> str:
    """who last touched the file a finding names: enough to address the owner of an inherited finding, nothing more"""
    paths = finding_paths(git_dir, tree, problem)
    if not paths:
        return ""
    g = ["git"] + (["-C", git_dir] if git_dir else [])
    r = subprocess.run(g + ["log", "-1", "--format=%h %an (%s)", "--", *paths], capture_output=True, text=True)
    who = r.stdout.strip() if not r.returncode else ""
    return f" (last touched by {who})" if who else f" (no commit for {'/'.join(paths)} in this history)"


def audit(git_dir: str | None, tree: str) -> list[str]:
    """every built-in-rule finding of one tree, [] if the tree is not a whole public checkout (then the CORE message says why)"""
    top = set(subprocess.run(["git"] + (["-C", git_dir] if git_dir else []) + ["ls-tree", "--name-only", "-z", tree],
                             capture_output=True, check=True).stdout.decode().split("\0")) - {""}
    probs = [f"the tree has no {c} (not a whole checkout of the public repository)" for c in CORE if c not in top]
    if not probs:
        probs += autorelease.audit_tree(git_dir, tree) + public_workflows.tree_problems(git_dir, tree)
    return probs


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--git-dir")
    ap.add_argument("--tree", default="HEAD^{tree}")
    ap.add_argument("--base", help="attribute the findings against this rev's tree (origin/main for a pull request)")
    a = ap.parse_args(argv)
    g = ["git"] + (["-C", a.git_dir] if a.git_dir else [])
    r = subprocess.run(g + ["rev-parse", "--verify", "-q", f"{a.tree}^{{tree}}"], capture_output=True, text=True)
    if r.returncode:
        print(f"PR AUDIT FAILED: {a.tree} is not a tree here")
        return 1
    tree = r.stdout.strip()
    probs = audit(a.git_dir, tree)
    n = len(subprocess.run(g + ["ls-tree", "-r", "-z", "--name-only", tree], capture_output=True, check=True).stdout.split(b"\0")) - 1
    own, inherited = probs, []
    base_note = ""
    if a.base and probs:
        rb = subprocess.run(g + ["rev-parse", "--verify", "-q", f"{a.base}^{{tree}}"], capture_output=True, text=True)
        if rb.returncode:
            base_note = f", --base {a.base} is not a tree here (nothing attributed)"
            print(f"PR AUDIT NOTE: --base {a.base} is not a tree here, nothing attributed")
        else:
            base_tree = rb.stdout.strip()
            base_probs = set(audit(a.git_dir, base_tree))
            own = [p for p in probs if p not in base_probs]
            inherited = [p for p in probs if p in base_probs]
            base_note = f"; {len(own)} of this tree, {len(inherited)} inherited from {a.base}"
            for p in inherited:
                print(f"PR AUDIT INHERITED: {p}  — already in {a.base}, not this pull request's{blame(a.git_dir, base_tree, p)}")
            for p in own:
                print(f"PR AUDIT: {p}")
    else:
        for p in probs:
            print(f"PR AUDIT: {p}")
    print(f"PR AUDIT {'FAILED' if probs else 'ok'}: {tree} ({n} files{base_note}; the built-in rules only, not a publishing check)")
    return 1 if probs else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
