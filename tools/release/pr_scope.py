#!/usr/bin/env python3
"""The scope of a pull request's engine work: does what it changed reach the binaries CI builds?

`.github/workflows/pr.yml` builds the engine for Linux, mingw, MSVC and macOS and runs the Windows binaries on real Windows. Those jobs
prove one thing: the merge commit builds and its unit tests pass. A pull request that changes only `docs/**` cannot change that answer --
GitHub checks out the merge of the pull request into main, and the merge's engine inputs are byte-identical to the base it merges into,
which main already built. Rebuilding it costs 6 to 9 minutes of wall clock and proves nothing new.

So the decision is computed from the diff, in the job, and printed -- not from a label someone forgot to set, not from `paths:` filters (a
job a filter removed reports `skipped`, and the aggregate `CI` is required to see `success` from every job), and not from a list of
files a human keeps current. Out of scope, the job runs one step, says why, and finishes green; `CI` and its "only success" rule stay as
they are, so no check name and no repository setting changes.

The lane is narrow on purpose. Only `docs/**` counts: the shipped archives stage the repository's root files
(`tools/release/build_launcher.sh` stages `launcher LICENSE NOTICE` and copies `LICENSE` and `NOTICE` into every package), so a pull
request touching `README.md`, `LICENSE`, a build script, `tools/**` or a top-level directory nobody thought of is in scope -- as is every
case this file cannot decide: a push, a merge queue run, a checkout without the base commit, an empty diff. Falling closed here costs a
slow pull request, which is the cheap direction to fall in; falling open would merge an unproven engine.

    python3 tools/release/pr_scope.py [--base REF] [--event NAME] [--github-output FILE]
"""
from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

# a path that cannot reach a binary: this repository's documents, and nothing else (see the module's docstring for why root files are out)
DOCUMENT = re.compile(r"^docs/[^/].*$")


def git(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(("git", *args), cwd=REPO, capture_output=True, text=True)


def changed_paths(base: str) -> list[str] | None:
    """the paths `base..HEAD` touches, or None when the comparison cannot be made (no such base -- a shallow checkout, a first commit,
    a failed git): the caller reads None as "no idea", which is answered by building"""
    if git("cat-file", "-e", f"{base}^{{tree}}").returncode != 0:
        return None
    result = git("diff", "--name-only", base, "HEAD")
    if result.returncode != 0:
        return None
    return [line.strip() for line in result.stdout.splitlines() if line.strip()]


def scope(paths: list[str] | None, event: str) -> tuple[bool, str]:
    """(whether the engine has to be built for this run, the reason to print)"""
    if event != "pull_request":
        return True, f"the run was triggered by {event or 'an unknown event'}, which has no pull request's diff to reason about"
    if paths is None:
        return True, "this checkout cannot name the pull request's base commit, so nothing can be concluded from its diff"
    if not paths:
        return True, "the diff against the base named no file, which is not a state to reason from"
    outside = [p for p in paths if not DOCUMENT.match(p)]
    if outside:
        return True, f"{len(outside)} of {len(paths)} changed path(s) are outside documents (e.g. {outside[0]})"
    return False, f"all {len(paths)} changed path(s) are documents, so the merge's engine inputs are identical to the base's"


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(prog="pr_scope.py", description="the scope of a pull request's engine work")
    ap.add_argument("--base", default="HEAD^1", help="the commit to compare against (GitHub's merge commit's first parent is main)")
    ap.add_argument("--event", default=os.environ.get("GITHUB_EVENT_NAME", "pull_request"), help="the workflow event name")
    ap.add_argument("--github-output", default=os.environ.get("GITHUB_OUTPUT"),
                    help="write `engine=yes|no` here ($GITHUB_OUTPUT of the step's id)")
    args = ap.parse_args(argv)
    paths = changed_paths(args.base) if args.event == "pull_request" else None
    build, why = scope(paths, args.event)
    print(f"SCOPE: engine build {'required' if build else 'not needed'} - {why}")
    if args.github_output:
        with open(args.github_output, "a", encoding="utf-8") as out:
            out.write(f"engine={'yes' if build else 'no'}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
