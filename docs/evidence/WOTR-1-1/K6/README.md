# K6 — Arena build snapshot and compiler launcher

Two separate aids, one rule: **neither is an acceptance result.** A snapshot carries the bytes a gate
run produced; a compiler launcher only makes recompiling cheap. No gate run may depend on either, and
no skip, miss or cache hit is reported as a pass.

## Snapshot: `tools/arena/build_cache.py`

Key = `HEAD^{tree}` + `BUILD_TYPE` + `OPENBFME_EXTRA_CXX_FLAGS` + `OPENBFME_CXX_LAUNCHER` + compiler
`--version` + the godot-cpp gitlink + platform + checkout path. A dirty or uncommitted tree has no key
at all, so only a committed state can be published or restored.

```sh
tools/arena/bootstrap.sh --runtime                                  # cmake, ninja, Godot
JOBS=2 tools/arena/build_engine.sh --clean --tests                  # the gate; writes .arena-cache-key
python3 tools/arena/build_cache.py save /tmp/openbfme-build-cache  # store outside the checkout
mv engine/build godot/bin godot/.godot /tmp/aside/                  # all three, or restore refuses
python3 tools/arena/build_cache.py restore /tmp/openbfme-build-cache
tools/arena/build_engine.sh --check
```

Measured on the standard 2 vCPU sandbox at `28d17a4` (`JOBS=2`): clean gate build 3447 s, `save` 10 s
(209 MiB archive from a 714 MiB build tree), aside + `restore` 9 s, and `restore` re-reads the
`libopenbfme_core.a` / `openbfme_tests` / `libopenbfme.so` SHA-256 against the values recorded before
the snapshot was taken. Rules that are enforced, not documented only: restore refuses while an output
directory exists, refuses when the archived manifest is for another tree, refuses when
`CMakeCache.txt` disagrees with the key, and rolls the directories back if `--check` or a hash
mismatches after install. Generate Godot imports before saving (the import is 4 s) and do not run a
build or an import concurrently with `save`/`restore`. The store is append-only by construction: one
file per key, published with `os.link`, never overwritten. Nothing goes to Drive and no archive, Drive
ID or download URL enters this repository.

## Compiler launcher: `OPENBFME_CXX_LAUNCHER`

```sh
export SCCACHE_DIR=/tmp/sccache-dir PATH=/tmp/k6-bin:$PATH
OPENBFME_CXX_LAUNCHER=sccache JOBS=2 tools/arena/build_engine.sh --clean --tests   # iteration only
```

Off by default and never for a gate. Measured at `28d17a4`: a matched 200-translation-unit sample
169.2 s without a launcher, 177.9 s with a cold cache (+5.1 %), 0.7 s warm (−99.6 %); a full
`--clean --tests` build 3447 s without a launcher against 18 s warm; priming the cache (2576 compiles,
94 MiB) costs one ordinary build, 3041 s. `ccache` was not installable in the sandbox (no reachable
apt mirror, release assets unreachable), so `sccache` 0.16.0 was measured; the same numbers are
expected for `ccache` and are not claimed for it. The `754.83 s` reference in
`docs/evidence/scriptcat-1.16.6.md` is a `pytest -n 4` release-file run, which no compiler launcher can
touch, and this card's cost gate is `retail: none`, so it was not repeated here.

Because the launcher is part of the key, a build directory configured with `sccache` can never be
published or restored as a plain one. A cache hit returns the same object bytes as a real compile
(verified on three translation units); the link is not byte-reproducible in either case, so the gate's
artifacts travel in the snapshot, not through the cache.

Full numbers, return codes, what was not run and the SHA: `report.md` in this directory.
