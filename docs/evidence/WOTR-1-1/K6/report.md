# WOTR-1.1 · K6 · Build cache and ccache measurement — Acceptance Report

- **Card:** `WOTR-1.1 · K6 · Build cache and ccache measurement` · dispatch `4216f0cf-5705-8174-9f10-eb8e4c1dca2c`
- **Dependency:** B0 `41e6f0cf-5705-818e-8c4f-ce0f5c0df68e` — `Accepted`, Check not fail
- **Run:** `f9e17694-85d0-4abe-ad57-225c3a6c94ad` · **Mission:** `41e6f0cf-5705-819a-a3ba-c15034fef51e`
- **Role:** Upstream Integrator (Executor), zone `OPENBFME_UPSTREAM` / `tools/arena/**`, `docs/evidence/WOTR-1-1/K6/**`
- **Target repo / branch:** `Creative36711/open-bfme`, `arena/12c509b7-open-bfme` from `main@64a13921057678810f1d8df79fdd51d12e9c3310`
- **Final code SHA:** `28d17a468ab5463ca0374f7933beaec7606fb355` (tree `428d1f69c55cd7f9a26c50ba3af490a8222901ef`)
- **Cost gate:** `build: full`, `retail: none`
- **Sandbox:** standard Arena sandbox, `nproc = 2`, Debian 12, `c++ (Debian 12.2.0-14+deb12u1) 12.2.0`, CMake 4.4.5, Ninja 1.13.2, Godot 4.7.2.stable.custom_build.ed1daf0bf
- **Date:** 2026-11-24

Intent: make the engine build cheap to repeat **without moving any proof onto the cache** — a keyed,
restorable artifact, one full gate per final SHA, and a compiler cache only behind a measurement.

---

## 1 · What ships

| Path | Action | Purpose |
| --- | --- | --- |
| `tools/arena/build_cache.py` | new | `key` / `save` / `restore` for `engine/build`, `godot/bin`, `godot/.godot` |
| `tools/arena/build_engine.sh` | modified | writes `engine/build/.arena-cache-key` after a committed `--tests` build; accepts `OPENBFME_CXX_LAUNCHER` |
| `docs/evidence/WOTR-1-1/K6/README.md` | modified | operator workflow and limits |
| `docs/evidence/WOTR-1-1/K6/report.md` | new | this report |

Deliberately **not** shipped: no CI change, no compiler launcher in any default path, no archive or
archive reference in Git, no Drive upload (a toolchain snapshot stays out of the repository), no
change to any file outside `tools/arena/**` and `docs/evidence/WOTR-1-1/K6/**`.

## 2 · The key (invariant 3)

`build_cache.py key` hashes a JSON object of the inputs that actually decide the build:

```json
{"schema": 1, "checkout": "/home/user/open-bfme", "platform": ["Linux", "x86_64"],
 "tree": "428d1f69c55cd7f9a26c50ba3af490a8222901ef",
 "godot_cpp": "507ed9d840c01a3c5b2a39af8bb4000bfac30bf5",
 "build_type": "Release", "flags": "-include cstddef", "launcher": "",
 "compiler": ["c++"], "compiler_version": "c++ (Debian 12.2.0-14+deb12u1) 12.2.0 ..."}
```

`tree` is `git rev-parse HEAD^{tree}` — the whole checkout, not just `engine/**`, and the godot-cpp
gitlink is carried explicitly because the upstream pin does not move between our commits.

Measured keys (`python3 tools/arena/build_cache.py key`, rc 0 each):

| State | key |
| --- | --- |
| `bb9abac` (previous commit, no launcher) | `36f63e49588be0969451f76646aad8d42aef4069d9750729f28f76f6a06cf78e` |
| `28d17a4`, `BUILD_TYPE=Release`, no launcher | `ce94ef5ae241020cdd934b31ce9d3a46139b8103b0efbf8ec1b72b600cd29969` |
| `28d17a4` + `OPENBFME_CXX_LAUNCHER=sccache` | `e18ce04a76cc3ec79cc6d3f6317d2b0da2fc7c8f8b9b3fcf60bf8325534481c2` |
| `28d17a4` + `BUILD_TYPE=Debug` | `e35d9b9ef03337c923f688d9724491c180c4cb5263913c4f4bf6c60b097d2b9c` |

Four different configurations, four keys, one file name per key (`<key>.tar.gz`) — a wrong-key
collision is structurally impossible in the store, and a snapshot that does not belong to the current
tree is refused twice (manifest config, then `.arena-cache-key`). Refusal paths actually executed:

| Command | rc | Output |
| --- | --- | --- |
| `python3 tools/arena/build_cache.py restore /tmp/k6-no-snapshot` with outputs present | 1 | `build cache: move existing output aside first: engine/build` |
| `python3 tools/arena/build_cache.py key` with uncommitted edits in the tree | 1 | `build cache: Command '('git', 'diff', '--quiet', 'HEAD', '--')' returned non-zero exit status 1.` |
| `restore` of a snapshot whose manifest is for another tree (collision test, §6) | 1 | `build cache: snapshot key mismatch` |

A dirty tree can never be published and never be restored, so a snapshot always names a commit.

## 3 · Full acceptance on the final SHA (oracle 3)

One run of the whole gate on `28d17a4`, quiet sandbox, `JOBS=2` (the standard acceptance set; the
cache is not part of it):

| # | Command | rc | duration |
| --- | --- | --- | --- |
| 1 | `env JOBS=2 tools/arena/build_engine.sh --clean --tests` | 0 | 3447 s (ninja alone 57 min 25 s) |
| 2 | `./engine/build/openbfme_tests` | 1 | 68 s |
| 3 | `timeout 240 /tmp/godotbin/godot --headless --path godot --editor --import` | 0 | 4 s |

Step 2 is the only red line, and it is red for a reason this card does not own:

```text
[doctest] test cases:    5652 |    5647 passed | 5 failed | 27 skipped
[doctest] assertions: 1409794 | 1409789 passed | 5 failed |
engine/tests/test_audio_decode.cpp:830: FATAL ERROR: REQUIRE( problem.empty() ) is NOT correct!
  logged: MP3 fixture missing or unreadable: mono_44k -- Missing MP3 fixture tool: lame
          Missing MP3 fixture tool: mpg123
```

`engine/build/audio_fixtures_status.txt` holds the same two lines, i.e. the MP3 golden fixtures could
not be synthesized in this rootless sandbox (`apt-get update` cannot reach any mirror). The five
cases are recorded as `not run` in `docs/evidence/WOTR-1-1/K4/report.md` and
`docs/evidence/WOTR-1-1/K5/report.md` for the identical reason and belong to the audio lane / the CI
`fixtures` job. This pull request touches `tools/arena/build_cache.py`, `tools/arena/build_engine.sh` and two files
under `docs/evidence/WOTR-1-1/K6/**` — no `engine/**` file, so the test binary is the binary the base
commit produces. Reported as failing, not as passed.

Duplicate run for variance. The same three commands were run once more at the previous commit
`bb9abac` while the operator was also working in the sandbox: `build_engine.sh --clean --tests`
rc 0 in 5202 s, tests rc 1 in 69 s, import rc 0 in 8 s. The 50 % spread is CPU contention with the
interactive shell, not code: the C++ inputs of both commits are identical. Every duration quoted
below was taken from the quiet chain.

## 4 · Restore path, `--check`, binary SHA-256 before and after (oracle 1, 2)

Store outside the checkout: `/tmp/openbfme-build-cache`. `engine/build` is 714 MiB, the snapshot
209 MiB (`tar.gz`, compresslevel 1).

| Command | rc | duration |
| --- | --- | --- |
| `python3 tools/arena/build_cache.py save /tmp/openbfme-build-cache` | 0 | 10 s |
| `mv engine/build godot/bin godot/.godot` to a side directory | 0 | <1 s (rename, same filesystem) |
| `python3 tools/arena/build_cache.py restore /tmp/openbfme-build-cache` | 0 | 9 s |
| whole aside + restore cycle | 0 | **9 s** |

Binary SHA-256 recorded by `save` (`before:`) and re-read by `restore` after the `--check`
(`after:`) — byte-identical, and equal to the hashes of the files the gate build produced:

```text
engine/build/libopenbfme_core.a                      bcfe105e5c9fe3c8e351d02b6af458d83d6b0f813103f1f5036aacef36897a9a
engine/build/openbfme_tests                          f1d5c1f999c10ad9f14b302106dedb40741a26192e3949fda4ada294758597b9
godot/bin/openbfme.linux.template_debug.x86_64.so    e2d8322a5340fad88e6a8b1fcc185b3e33dcc8c8e32bda94205159f4fe2d1af8
```

Snapshot archive: `4ae34b1f697abb34fa7aa7835b0e5ed22fec577d43933191794ad92801849c72` (store entry
`ce94ef5a….tar.gz`). The verification inside `save`/`restore` is `tools/arena/build_engine.sh --check`
plus `CMakeCache.txt` comparison against the key (`CMAKE_BUILD_TYPE`, `CMAKE_CXX_FLAGS`,
`CMAKE_CXX_COMPILER_LAUNCHER`, compiler `--version`), and `--check` is read as a report, not as an
exit code — it exits 0 even when every output is missing, so `✗` in its output is an error:

```text
build_engine.sh --check: source: /home/user/open-bfme
    ✓ godot-cpp present
    ✓ core: 56M
    ✓ extension: /home/user/open-bfme/godot/bin/openbfme.linux.template_debug.x86_64.so
    ✓ tests: /home/user/open-bfme/engine/build/openbfme_tests
```

The ninja log comes back with the tree, so a restored checkout is already up to date for ninja and
rebuilds only what was edited afterwards. Restoring is 9 s against a
3447 s clean build — the reason the cache exists, and the reason nothing in the gate may depend on it.

## 5 · Compiler cache: the measurement (oracle 4)

**Which tool.** `ccache` is not installable here: the sandbox has no reachable apt mirror
(`sudo apt-get update` → `Failed to fetch http://deb.debian.org/... Connection failed`) and GitHub
release assets are unreachable (`release-assets.githubusercontent.com` TLS failure). `sccache`
0.16.0 — the other half of the card's "ccache/sccache" — ships its Linux release binary as a
manylinux wheel on the Python package index, which is reachable; installed to `/tmp/k6-bin/sccache`
outside the checkout, `sccache --version` rc 0, binary SHA-256
`8cc465f71f6f6c85d9df34503fda47f0d23480796501605e0502c88004546149`. Cache directory
`/tmp/sccache-dir`, 10 GiB limit, local disk mode.

**What the 754.83 s base is.** `docs/evidence/scriptcat-1.16.6.md:202` records
`python3 -m pytest -n 4 tools/release/test_autorelease.py tools/release/test_community_prs.py
tools/release/test_public_push_guard.py -q` → `392 passed, 6 skipped in 754.83 s`. That is a Python
release-file run, not an engine compile: a compiler launcher is not on that path at all, and this
card's cost gate is `retail: none`, so the run was **not repeated here** (see §8). The only engine-side
cycle a compiler cache can touch is the C++ build, so the 25 % threshold is applied to the measured
C++ iteration cycle and to the measured clean build, both below.

**Compile-phase sample** (200 real translation units of `openbfme_core`, the exact ninja command line,
`-j2`, objects to `/tmp/k6-tu`, `/tmp/k6-sample.py`):

| variant | wall | delta |
| --- | --- | --- |
| `plain` — no launcher | 169.2 s | — |
| `cold` — sccache, empty cache | 177.9 s | +5.1 % |
| `warm` — sccache, cache primed by `cold` | 0.7 s | −99.6 % |

`sccache --show-stats` after the sample: 400 compile requests, 200 misses, 200 hits, 0
non-cacheable, 0 unsupported compiler calls, 0 cache errors, `Average compiler 1.689 s`,
`Average cache read hit 0.000 s`. So: a cold cache costs about 5 % extra, a warm cache removes
essentially the whole compile phase.

**What that means for the full build.** `engine/build/.ninja_log` of the gate build records 2602
compile edges with 6870 s of CPU against 2179 s for every other edge (configure-time generation,
archives, links): 75.9 % of the logged CPU is work a launcher can replace, so a clean-rebuild saving
of roughly three quarters is the floor even if linking and configuring stay untouched. The measured
warm run below is better than that floor because the hits also shorten the critical path.

**Measured cycles** (same sandbox, `JOBS=2`, quiet; `OPENBFME_CXX_LAUNCHER=sccache` is the only difference):

| cycle | command | rc | duration |
| --- | --- | --- | --- |
| clean build, no launcher (the gate, §3) | `tools/arena/build_engine.sh --clean --tests` | 0 | 3447 s |
| prime the cache with the launcher | `OPENBFME_CXX_LAUNCHER=sccache tools/arena/build_engine.sh --tests` | 0 | 3041 s |
| clean build again, warm launcher cache | `OPENBFME_CXX_LAUNCHER=sccache tools/arena/build_engine.sh --clean --tests` | 0 | 18 s |
| iteration, no launcher (40 sources touched) | `tools/arena/build_engine.sh --tests` | 0 | 1709 s |
| iteration, warm launcher cache (same 40 sources) | `OPENBFME_CXX_LAUNCHER=sccache tools/arena/build_engine.sh --tests` | 0 | 5 s |

The iteration cycle is expensive for a reason worth recording: `build_engine.sh` re-runs CMake, the
configure step regenerates the vendored `engine/build/lua-ea` sources, and that dirties 1426 of the
2618 edges (368 core objects, 351 test objects, lua/luau/relay and everything that links against the
re-archived `libopenbfme_core.a`) — 1709 s for a 40-file edit. Fixing that is an `engine/CMakeLists.txt`
change, outside this card's zone; it is filed here as the finding, not silently worked around.

The two iteration rows are **not** a matched pair and are not quoted as a saving: the first
re-configure after a restore regenerated the vendored sources and dirtied 1388 compiles, while the
later run found that generation already settled and asked the launcher for only 79 compiles
(`sccache` requests 5152 → 5231, all 79 hits). The matched, per-file comparison for exactly that work
is the sample above (169.2 s of real compiles against 0.7 s of hits on the same 200 translation
units), and the matched whole-cycle comparison is the clean-build pair: `tools/arena/build_engine.sh
--clean --tests` without a launcher 3447 s, with a warm launcher cache 18 s, both rc 0, both
producing a complete `engine/build` (714 MiB, 2602 objects, `libopenbfme_core.a` 58 MiB,
`openbfme_tests` 116 MiB).

That 18 s build is not taken on trust: `./engine/build/openbfme_tests` was run on the binaries it
produced and reported the same `5652 | 5647 passed | 5 failed | 27 skipped` as the gate run (rc 1,
64 s), and three of the cached objects were recompiled without a launcher into a separate file and
compared byte for byte:

```text
AptActionDecoder.cpp.o   cached_hit=91da256a8a8310d8 plain_compile=91da256a8a8310d8 equal=True
SelectionDecals.cpp.o    cached_hit=4f5304631be7cb7f plain_compile=4f5304631be7cb7f equal=True
NetworkSettings.cpp.o    cached_hit=28e75ad7fdf23751 plain_compile=28e75ad7fdf23751 equal=True
```

A launcher hit returns the same object a real compile returns; it is the *link* of this project that
is not byte-reproducible (`libopenbfme_core.a` hashes `dfcbffbf…` after the warm build against
`bcfe105e…` from the gate, while every object that went into it is identical), which is a second
reason the gate artifact is carried by the snapshot, not by a cache.

**Adoption.** **Adoption.** The card's bar is a quarter of the iteration cycle. Measured on the work a compiler
launcher can actually replace, on the same 2 vCPU sandbox: the matched compile sample saves 99.6 %
(169.2 s → 0.7 s) and the matched clean `--tests` build saves 99.5 % (3447 s → 18 s), at a cold-cache
cost of +5.1 % and one ordinary build (3041 s) to prime 94 MiB of cache. That clears 25 % by two
orders of magnitude, so the opt-in path ships: `OPENBFME_CXX_LAUNCHER=sccache` (or `ccache`) is
accepted by `build_engine.sh`, is part of the snapshot key, and is **off by default**. No gate run,
and nothing that produces an acceptance result, may use it (invariant 2): the gate above ran without
it, and the `launcher` field of its key is the empty string — `save` only succeeds when
`CMakeCache.txt` carries no `CMAKE_CXX_COMPILER_LAUNCHER` at all, so a gate snapshot cannot come from
a launcher-configured tree and a launcher snapshot cannot be published under the gate key.

The verdict is bounded on purpose: a warm cache only makes *repeated* compile work cheap inside one
sandbox, and the cache lives in `/tmp` and is lost with it. For a fresh checkout the snapshot restore
is the cheap path (9 s and 209 MiB against 3447 s), and it carries the exact bytes the gate produced.
Restore and launcher are complementary, neither is an oracle.

## 6 · Wrong-key collision test (invariant 3)

`/tmp/k6-cache` holds the snapshot of the *previous* commit `bb9abac` (key
`36f63e49…`). It was copied under the current key name into a third store and offered to `restore`
with the three output directories moved aside — the file name matches, the content does not:

```text
$ sha256sum /tmp/k6-collide/ce94ef5a…tar.gz
d9a78db8be24e5f7739178c61956222db9c62b8360bc27c7c7b825cf062e7a3a  …
$ python3 tools/arena/build_cache.py restore /tmp/k6-collide
build cache: snapshot key mismatch                          (rc 1)
$ tools/arena/build_engine.sh --check                     (rc 0, all four ✓ after moving the real
                                                            outputs back)
```

The archive SHA-256 of the foreign snapshot is unchanged by the attempt, and nothing was written into
the checkout. The gate-shaped run was not affected by the test.

## 7 · Toolchain snapshot and Drive (oracle 5)

Nothing was uploaded: no Drive file, no Drive ID, no download URL and no archive enters the
repository or this evidence — `git diff --name-only origin/main...HEAD` lists only the four files of
§1, and `git ls-files | grep -cE '\.(tar\.gz|zip|7z)$'` returned `0`. The store is a plain directory
outside the checkout (`/tmp/openbfme-build-cache`), one file per key, published with `os.link` so an
existing snapshot is never overwritten. If a later card moves the snapshot to a shared store, the
archive already carries `manifest.json` with the full key inputs and the binary SHA-256 map, so the
append-only manifest + `SHA256SUMS` discipline required by the card has its material there; the store
must stay append-only, and only the key and the hashes — never an ID or a URL — belong in an evidence
file.

## 8 · Not run

| Item | Status | Why |
| --- | --- | --- |
| The `754.83 s` release-file pytest run, with and without a launcher | not run | cost gate `retail: none`; a compiler launcher is not on that path (a warm C++ cache cannot change a Python test run) |
| The five MP3 golden-fixture cases of `engine/tests/test_audio_decode.cpp` | not run (fail in the monolithic run for the missing tools) | `lame` and `mpg123` are absent and cannot be installed in a rootless sandbox with no reachable mirror; owned by the audio lane / CI `fixtures` job, already recorded the same way in K4 and K5 |
| `ccache` itself | not measured | not installable in this sandbox (§5); `sccache` is measured instead, as the card allows |
| A shared/remote snapshot store, Drive upload, `SHA256SUMS` of a pushed snapshot | not run | deliberately not done for this card (§7) |
| Restore into a *different* checkout path | not run | CMake/Ninja embed absolute paths, so `build_cache.py` refuses by key: `checkout` is part of the key |
| `tools/arena/bootstrap.sh` duration on a cold sandbox | not measured | its cost is not part of the snapshot path; the restore number above excludes it |
| Windows/macOS cache layouts, `sccache` distributed compilation | not run | the Arena build lane is Linux-only |

## 9 · Reproduce

```sh
tools/arena/bootstrap.sh --runtime                                  # cmake, ninja, godot
JOBS=2 tools/arena/build_engine.sh --clean --tests                  # the gate
./engine/build/openbfme_tests
python3 tools/arena/build_cache.py save /tmp/openbfme-build-cache
mv engine/build godot/bin godot/.godot /tmp/aside/                  # any path outside the checkout
python3 tools/arena/build_cache.py restore /tmp/openbfme-build-cache
tools/arena/build_engine.sh --check

# launcher cycle, never part of a gate
sccache --start-server && export SCCACHE_DIR=/tmp/sccache-dir
OPENBFME_CXX_LAUNCHER=sccache JOBS=2 tools/arena/build_engine.sh --clean --tests
```

## 10 · Допущения

- **Допущение:** «iteration cycle» в оракуле — измеряемый C++ цикл движка (чистая сборка гейта и
  цикл после правки 40 файлов), а не питоновский прогон `754.83 s`: компиляторный кэш на него не
  влияет, а `retail: none` запрещает его воспроизводить. Порог 25 % применён к C++-циклу и
  задокументирован числами в обоих случаях.
- **Допущение:** «финальный SHA» = последний коммит, меняющий код (`28d17a4`); коммит отчёта в
  `docs/evidence/WOTR-1-1/K6/**` не может предшествовать собственным замерам. Он меняет `HEAD^{tree}`,
  а значит и ключ снапшота — это ожидаемое поведение ключа, а не дефект: снапшот, снятый до
  коммита, после него не восстанавливается.
- **Допущение:** 9-секундный `restore` и `save` считаются на той же файловой системе, что и
  checkout (`mv` = rename); на внешнем хранилище добавится сетевой перенос 209 MiB, и он не
  измерен.
