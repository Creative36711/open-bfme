# Synchronising WOTR Remaster with OpenBFME nightly

WOTR Remaster keeps the current OpenBFME tree at the repository root and pins
its exact source commit in `OPENBFME_UPSTREAM`. It does **not** keep a nested
`upstream/` copy.

## Why the upstream commit is not a Git parent

`Creative36711/open-bfme` is an ordinary repository rather than a formal GitHub
fork. OpenBFME's historical commits reference Git LFS objects owned by the
upstream repository. Making that history a parent caused GitHub to reject a
push with `GH008` for 234 unknown LFS objects, even though the current source
tree itself contains no LFS pointers.

The repository therefore imports the current source **tree**, but not the old
commit graph. This is not a blind file copy: updates use an explicit three-way
merge with these inputs:

```text
base   = OPENBFME_UPSTREAM (previous accepted upstream SHA)
ours   = current WOTR Remaster HEAD
theirs = newly fetched OpenBFME SHA
```

That has the conflict behaviour of a normal downstream merge while keeping
unreachable historical LFS objects out of pushes.

## Remotes and commands

One-time local setup:

```bash
git remote add upstream https://github.com/Open-BFME/openbfme-godot.git
```

Inspect an available update without changing files:

```bash
tools/arena/sync_upstream.sh --check
```

Stage the explicit three-way tree merge:

```bash
tools/arena/sync_upstream.sh
```

The script:

1. fetches `upstream/main` without tags;
2. merges old upstream / downstream HEAD / new upstream at tree level;
3. leaves real conflicts in the index for review;
4. rejects a resulting tree containing Git LFS pointers;
5. updates `OPENBFME_UPSTREAM` only after a conflict-free merge;
6. stages changes but never commits or pushes automatically.

## Pin, do not float at runtime

Development may update frequently, but every build is validated against the
SHA in `OPENBFME_UPSTREAM`. Never download an unknown `main` during build or
runtime. Record the accepted SHA/tag in release notes.

## Conflict-minimising rules

1. Put new deterministic code under `engine/src/GameLogic/WotrRemaster/` and
   new presentation under `godot/wotr_remaster/` where possible.
2. Keep registration/binding edits in upstream files small and mechanical.
3. Do not reformat or rename unrelated upstream code.
4. Keep each upstream sync in its own commit, separate from WOTR features.
5. Propose generally useful, retail-faithful fixes upstream; retain intentional
   remaster rules downstream.
6. Never vendor another OpenBFME copy in a subdirectory.

## Update gate

Accept a staged update only after:

1. configure and compile core/GDExtension;
2. upstream source-only tests;
3. deterministic WOTR rule/replay/save hash tests;
4. retail archive policy check (`107/107` BFME2 and `106/106` RotWK);
5. boot/menu/strategic-map smoke test through SwiftShader;
6. one tactical setup → battle result → strategic apply round-trip;
7. save/resume and hero rank/experience persistence;
8. strategic map/HUD screenshot comparison.

After an Arena reset, group retail-dependent stages into one session rather
than restoring 5.249 GiB separately for each check.

## Automation target

A scheduled GitHub Action may report a new upstream SHA and open a sync issue or
pull request, but it must not merge nightlies automatically. The update gate and
a human review of conflicts remain required.
