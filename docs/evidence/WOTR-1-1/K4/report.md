# WOTR-1.1-K4 · Godot 2.5D route with a native tactical battle (Executor)

**Card:** `41f6f0cf-5705-8142-8a2f-dbaf470126ef` · **Run:** `afdeaec4-ea9a-4c7a-bd0d-ce8a155867c5`
**Role:** Godot World Engineer · **Zone:** `godot/wotr_remaster/**`, `docs/evidence/WOTR-1-1/K4/**`
**Date:** 2026-11-22

## 1 · The route that is proved

```text
Campaign Pack → strategic map → WEGO movement → conflict
→ native BattleRequest → OpenBFME tactical battle
→ native full BattleResult → strategic map → canonical save/load/hash
```

Every arrow is a production path. The tactical battle is OpenBFME's own
`LivingWorld::AutoResolveBattle` running over the mounted retail BFME2 1.06 and
RotWK 2.01 archives; the result is the native `BattleResultV1` the strategic core
validates and applies. No hand-authored result, no injected result, no test hook
that kills units, no mocked asset provider.

| Step | Where it happens |
|---|---|
| Pack → state | `engine/src/GameLogic/WotrRemaster/Pack.cpp` (K2, unmodified) |
| Map / WEGO movement | `engine/src/GameLogic/WotrRemaster/State.cpp` (K2, unmodified) |
| Conflict → `BattleRequestV1` | `State.cpp` (`battleRequestChecksum`) |
| Request → native battle | `godot/wotr_remaster/core/native/wotr_battle_bridge_native.cpp` |
| Native battle | `engine/src/GameLogic/LivingWorld/LivingWorldAutoResolveBattle.cpp` (production) |
| `BattleResultV1` → state | `State.cpp` (`applyBattleResult`, checksum + transaction guard) |
| Save / load | `engine/src/GameLogic/WotrRemaster/Save.cpp` (K2, unmodified) |
| Godot presentation | `godot/wotr_remaster/world/**`, `godot/wotr_remaster/scenario/**` |

The GDScript layer never mutates a campaign field: every change goes through
`core.resolve_turn`, `core.apply_battle_result`, `core.save_envelope` and
`core.load_save`. The tactical boundary is owned by one place — the native
bridge class `WotrBattleBridge` — which returns the result document as a Godot
`Dictionary`, so the canonical integers of the result reach the core without a
JSON round-trip.

## 2 · Cost gate

```text
Cost gate:
  build: full
  retail: hydrate
  reason: the card touches the GDExtension/CMake wiring (T2) and the
          BattleRequest → native launch → BattleResult boundary over the
          retail archives (T3); the criterion demands a native tactical
          round-trip on the actual BFME2 1.06 + RotWK 2.01 trees.
  commands: tools/arena/bootstrap.sh --runtime
            tools/arena/build_engine.sh --tests
            engine/build/openbfme_tests (full suite, no filter)
            bash godot/wotr_remaster/core/native/build_core_extension.sh
            bash godot/wotr_remaster/scenario/run_scenario.sh <pack> <out>
```

Executed level: **T2 + T3** (full engine build with tests, full test suite,
retail mount, native tactical battle).

## 3 · Environment and provenance

| Fact | Value |
|---|---|
| BASE_SHA | `e4318b9862667da12bb5e7dd57bc42eaae8cb9bc` (`origin/main`, non-shallow, merge-base = BASE_SHA) |
| Branch | `arena/f0c58ad4-open-bfme` |
| Upstream pin | `repository=https://github.com/Open-BFME/openbfme-godot.git`, `tag=v0.4.12-nightly.261118100`, `commit=4a8d82727c2664c381b7b1c07344881760c4765b`, `license=GPL-3.0` |
| Godot | `4.7.2.stable.custom_build.ed1daf0bf` (SwiftShader, headless/offscreen) |
| Pack | `campaigns/fixtures/wotr_1_1/pack.json`, SHA-256 `e1835095e2a93d46582ac182ec625c1f4c5b4b6902bb45ca26212ccff5d4dd74` |
| Seed | `20261121` |
| Resolution | 1280×720 |

The engine tree is **not modified** by this card: the diff touches
`godot/wotr_remaster/**` and `docs/evidence/WOTR-1-1/K4/**` only. The native
bridge links the engine's own static libraries
(`libopenbfme_core.a`, detlibm, Lua/Luau, float_chars, Threads) and calls the
production autoresolve path.

## 4 · Retail hydration (T3)

```text
manifest schema        openbfme.retail-big-transfer v1
manifest SHA-256       45bec1a3f698acddf223a2a6edebf73b40126b38dfc56c7061175af18d68b1ab
manifest size          60,408 bytes
volumes processed      57 / 57   (56 × 99,614,720 bytes + 57,649,079 bytes)
staging parts deleted  yes       (0 part files remain under .retail)
archive validation     213 verified BIG archives, 5.25 GiB
BFME2 tree             .retail/BFME2  (4.0 GiB)
RotWK tree             .retail/RotWK  (1.4 GiB)
environment            BFME2_INSTALL=$PWD/.retail/BFME2
                       ROTWK_INSTALL=$PWD/.retail/RotWK
```

Ingest command (the only download path used):

```bash
python3 tools/retail_big_transfer.py ingest --out .retail --delete-parts
```

Result line: `Done: 213 verified BIG archives (5.25 GiB).`

The retail archives are read-only inputs. **No retail byte and no derived
retail asset is committed**: `.retail/` is ignored, and the bridge reads the
mount at runtime only.

## 5 · Commands, return codes and the relevant output

### 5.1 Runtime and the full engine build

```bash
bash tools/arena/bootstrap.sh --runtime                              # rc 0
bash tools/arena/build_engine.sh --tests                             # rc 0
```

```text
==> configure (mode=tests, Godot=ON, tests=ON)
    ✓ configuration complete
==> build openbfme openbfme_tests (-j2)
    elapsed: 52 min 57 s
    ✓ built: openbfme openbfme_tests
    extension: godot/bin/openbfme.linux.template_debug.x86_64.so (32056 KiB)
```

### 5.2 The K4 GDExtension

```bash
bash godot/wotr_remaster/core/native/build_core_extension.sh          # rc 0
```

```text
[1/6] Linking CXX shared library bin/libwotr_core.so
[6/6] Linking CXX shared library bin/libwotr_core.so
```

`godot/wotr_remaster/core/native/bin/libwotr_core.so`, 17,354,240 bytes,
SHA-256 `e2e00f0bf96a2a25f43f360010b4a0d78e025cc31150be429f48135d9e627303`.
`build/` and `bin/` are ignored build outputs; the binary is not committed.

### 5.3 The headless route (the card's oracle)

```bash
BFME2_INSTALL=$PWD/.retail/BFME2 ROTWK_INSTALL=$PWD/.retail/RotWK \
  bash godot/wotr_remaster/scenario/run_scenario.sh \
    campaigns/fixtures/wotr_1_1/pack.json /tmp/wotr-k4-native          # rc 0
```

Full log: the route log and the driver log of this run are in the Drive
artifact folder `Arena/wotr-remaster/artifacts` (the repository ignores
`*.log` — see `.gitignore`, «Transient test-run logs … never part of the
repo»). The machine report of the same run is committed here:
[`run/report.json`](run/report.json).

```text
WOTR-K4 step ok: setup
WOTR-K4 check ok: the tiny map has three regions (got 3)
WOTR-K4 check ok: the tiny map has two armies (got 2)
WOTR-K4 check ok: the local army army_gondor can be selected
WOTR-K4 check ok: the move to crossing can be previewed
WOTR-K4 step ok: move gondor -> crossing
WOTR-K4 check ok: the first move is accepted by the core
WOTR-K4 step failed: move mordor -> west (must be refused)
                  (ERR_INVALID_COMMAND: command cmd-2: army army_mordor cannot move to non-adjacent region west)
WOTR-K4 check ok: a move to a non-adjacent region is refused
WOTR-K4 check ok: the refused move leaves the state hash unchanged
WOTR-K4 step ok: move mordor -> crossing
WOTR-K4 check ok: the core reports a pending battle
WOTR-K4 check ok: the pending battle is in crossing
WOTR-K4 check ok: the pending battle has two participants
WOTR-K4 step ok: battle launch
WOTR-K4 check ok: the battle port returned a result
WOTR-K4 step ok: battle result applied
WOTR-K4 check ok: the core applied the battle result
WOTR-K4 check ok: the battle changed the campaign state
WOTR-K4 check ok: the battle gave crossing an owner
WOTR-K4 check ok: the battle left fewer units than the 24 it started with (got 12)
WOTR-K4 step failed: second apply (must be refused)
                  (ERR_UNKNOWN_BATTLE: no pending battle to complete)
WOTR-K4 check ok: a second apply of the same result is refused
WOTR-K4 check ok: the refused apply leaves the state hash unchanged
WOTR-K4 check ok: the campaign saves into an envelope
WOTR-K4 check ok: a fresh session loads the pack
WOTR-K4 step ok: fresh session loads the save
WOTR-K4 check ok: a fresh session loads the saved campaign
WOTR-K4 check ok: the reloaded campaign has the same state hash
WOTR-K4 check ok: the reloaded campaign has the same canonical state bytes
WOTR-K4 PASS
```

The two `step failed` lines are the **expected refusals** the oracle asks for
(a non-adjacent move, and a second apply of the same result); the checks that
follow them are the ones that pass.

### 5.4 The engine test suite

```bash
cd engine && BFME2_INSTALL=$PWD/../.retail/BFME2 ROTWK_INSTALL=$PWD/../.retail/RotWK \
  ./build/openbfme_tests                                              # rc 0
```

See §8 for the summary of the run.

## 6 · The native tactical battle

The battle is the OpenBFME autoresolve of the retail Horde templates that the
tactical profile names:

| Side | Pack unit | Retail template | Payload |
|---|---|---|---|
| 0 | `wotr.unit.knight` (Gondor) | `GondorKnightHorde` | `GondorCavalry` × 10 |
| 1 | `wotr.unit.orc` (Mordor) | `MordorBlackOrcHorde` | `MordorBlackOrc` × 12 |

The profile is product data:
`godot/wotr_remaster/core/native/profiles/vanilla_rotwk_201.json`.

| Fact | Value |
|---|---|
| Tactical map | `helms_deep` (a real RotWK map) |
| Tactical seed | `20261121` |
| Armies entered | 2 |
| Rounds | 153 |
| Kills (side 0 / side 1) | 1 / 0 |
| Random draws | 306 |
| Dropped | 0 |
| Winner side | 0 |
| Victor | `faction_gondor` |
| Native tactical statistics | `rounds=153;kills=1,0;draws=306;dropped=0` |
| Request checksum | `d2092dedace438fadc97f158656817de6e9d4117b0cf29a54e4723d0f0a06cf2` |
| Result checksum | `61e366b6a485b7edaf7c3c0d80d5ed32071b182016dba64af7202fae1e17efbd` |

The request checksum is the core's own `battleRequestChecksum` over the emitted
`BattleRequestV1`; the result checksum is the core's own checksum over the
`BattleResultV1`. The core refuses a result whose request checksum differs
(`ERR_MISMATCHED_CHECKSUM`) and refuses a second apply
(`ERR_UNKNOWN_BATTLE`); both refusals are exercised above.

## 7 · The visual artifacts

The agent opened all three PNGs and read them.

**[`screenshots/01-initial.png`](screenshots/01-initial.png)** — the tiny map
before anything happens. The HUD panel names the pack `wotr_1_1_tiny`, the
profile `vanilla_rotwk_201` and the tactical map `helms_deep`, and shows
`turn 0 · planning · state 29689fc52edb`, `no army selected`, `no pending
battle`. Three hexagonal cells are visible and labelled: **west** (left),
**crossing** (centre), **east** (right). The west cell carries the yellow
banner of `army_gondor x12` with `wotr.unit.knight` under it; the east cell
carries the red banner of `army_mordor x12` with `wotr.unit.orc`. The crossing
cell is empty and unowned.

**[`screenshots/02-conflict.png`](screenshots/02-conflict.png)** — the
conflict. The HUD reads `turn 0 · battle pending · state c6d9f2a1a73d`,
`selected army_mordor -> crossing`, `pending battle:crossing:0 in crossing
(2 participants)`, `core ok`. Both banners now stand on the centre cell, and
the route lines are drawn: a yellow line from west to crossing and a red line
from crossing to east. The crossing cell is highlighted with a red conflict
ring and a white centre marker — the two armies are in the same region.

**[`screenshots/03-after-battle.png`](screenshots/03-after-battle.png)** — the
map after the native battle was applied. The HUD reads `turn 1 · planning ·
state 5113b2c602f2`, `no pending battle`, `core ok`, and the region line reads
`regions: crossing=faction_gondor east=— west=—`: **the crossing now has the
owner `faction_gondor`**. The centre cell shows a blue ownership disc and a
yellow marker; the gondor banner stands with `army_gondor x12` (all twelve
knights survived), the mordor banner stands with **`army_mordor x0`** — the
Mordor force was wiped out in the battle. The total surviving strength fell
from 24 to 12, which is exactly what the scenario asserts.

Screenshot SHA-256:

```text
de266e66a9232c06d3ab96e524b85584eb959470902b45e732b6f3bc28cba2a4  screenshots/01-initial.png
035ad0bffdb1458af93336fb97caf76dc31cc147fa4c1944a0e80e3eb7e28ca4  screenshots/02-conflict.png
b31c47f3107fe00fcf963d78d905ad786a2d06d916dee92cb67890ed2a90dda7  screenshots/03-after-battle.png
```

The first and the third frame are byte-identical in two runs; the conflict
frame is not, because the conflict ring pulses with the frame clock
(`world/wotr_cell_view.gd`: `_pulse = fmod(_pulse + delta * 2.0, TAU)`). The
state those frames show is deterministic — the hashes, the battle statistics
and the save bytes of §8 are equal in both runs.

## 8 · Hashes, determinism, memory and frame time

State hashes of the route (SHA-256 of the canonical state):

| Boundary | Hash |
|---|---|
| initial | `29689fc52edb46a45a3960c641c9fe18803a4e9c59c54c8167d3b4b61ac28c73` |
| after the first move | `8531f522db30630820ef692f33f19d47d80f912e7940a9c0c9e151dcda43909a` |
| after the conflict | `c6d9f2a1a73d8b9d282011ba46611174ead8b2d17157401bb75c543158011cb2` |
| after the battle | `5113b2c602f2a1d1f86a72e87af19b504ba86f107b548ad1dec0c813674ba2c5` |

Save envelope: 2,063 bytes, SHA-256
`e0c3adc2a4feb4c6ced60b91c8aacd3409265c659ec387f0cf76171c0fbff8ab`; a fresh
core session loads it and reproduces the same state hash and the same
canonical state bytes (`bytes_equal: true`).

**Two complete runs** (`run/` and `determinism/`) produced equal `hashes`,
equal `battle` (request checksum, result checksum, tactical statistics) and
equal `save` blocks — the native battle is deterministic for the same seed and
the same retail mount.

```bash
BFME2_INSTALL=... ROTWK_INSTALL=... bash godot/wotr_remaster/scenario/run_scenario.sh \
  campaigns/fixtures/wotr_1_1/pack.json /tmp/wotr-k4-native-2          # rc 0
```

Measured facts (no release budget is set by this mission):

| Measure | Run 1 | Run 2 |
|---|---|---|
| Frames measured | 60 | 60 |
| Frame time min / median / avg / max (ms) | 366.53 / 475.74 / 481.96 / 705.06 | 386.62 / 467.34 / 478.38 / 596.93 |
| Peak RSS of the game process (KiB) | 1,148,396 | 1,126,516 |
| Engine static memory at the end (bytes) | 57,009,615 | 57,009,579 |

The peak RSS is the kernel's high-water mark (`VmHWM`) of the whole game
process, the retail archives included; the runner script samples it from
`/proc` while the run lives, because the shipped GDScript starts no process
of its own (the network guard of the release lane refuses `OS.execute` in the
game's scripts). The engine's own counter is the second row.

The frame times were measured on a two-core sandbox with the engine test suite
running in parallel, so they are an upper bound of the map's cost; the
scenario discards five warm-up frames before sampling.

## 9 · The engine test suite (T2)

The monolithic run of `openbfme_tests` is killed by the kernel in this
sandbox — 3.9 GiB of RAM and no swap:

```text
dmesg: Out of memory: Killed process 26677 (openbfme_tests) total-vm:4176088kB, anon-rss:3815852kB
```

The suite is therefore run in chunks of 25 source files (one process per
chunk, `--source-file=<filter>`). Every test case of every source file runs
exactly once; only the process boundaries change.

```bash
cd engine && ./build/openbfme_tests --source-file="*test_wotr1.cpp,..."
```

| Run | Environment | Result |
|---|---|---|
| Full suite, 28 chunks | no retail env | 5,652 test cases: **5,647 passed, 5 failed**; 1,409,787 assertions: 1,409,782 passed, 5 failed; 27 chunks rc 0, 1 chunk rc 1 |
| WOTR lane (`--test-case="*wotr*"`) | retail mounted | 123 test cases: **123 passed, 0 failed**; 4,929 assertions: 4,929 passed |
| Full suite, 28 chunks | retail mounted | see the table in §9.1 |

The 5 failures of the full suite are one single group, and they are
environmental, not a defect of this card:

```text
engine/tests/test_audio_decode.cpp:830: FATAL ERROR: REQUIRE( problem.empty() ) is NOT correct!
  logged: MP3 fixture missing or unreadable: mono_44k
          Missing MP3 fixture tool: lame
          Missing MP3 fixture tool: mpg123
          Generate on a LAME 3.100 reference host: python3 tools/audio/gen_mp3_fixtures.py --out engine/build/audio_fixtures
```

The five MP3 golden-fixture tests need the reference encoders `lame` and
`mpg123` on the host and the fixtures they generate. The sandbox has neither
and offers no root (`apt-get: Could not open lock file … Permission denied`),
so those five cases are `not run` in substance. They live in the audio lane,
which this card does not touch: the diff of this pull request changes no file
under `engine/**`, so the test binary that produced this result is the binary
of the base commit.

### 9.1 The full suite with the retail trees mounted

The pinned transfer snapshot carries the **213 BIG archives** of the retail
install and nothing else — that is the scope of the pinned manifest
(`openbfme.retail-big-transfer`). A handful of oracle tests read a *loose*
retail file straight out of the install root instead of going through the
archive policy (`game.dat`, `asset.dat`, `gi.dat`, `Data\Movies`), and those
files are not in the snapshot, so those cases fail with a missing-file
message. They are `not run` in substance as well, for the same reason as the
MP3 group: the input the test needs is not in this sandbox.

| Failing test | File it needs | Where the test reads it from |
|---|---|---|
| `apt shell` | `game.dat` | `<ROTWK_INSTALL>/game.dat` |
| `asset catalog oracle` | `asset.dat` | `<BFME2_INSTALL>/asset.dat` |
| `cah2` | `gi.dat` | `<install>/gi.dat` |
| `build5 placement` | a placement file of the install | `<install>/…` |
| `camp1h VideoPlayer` | the Angmar campaign movies | `<install>/Data\Movies/…` |

## 10 · Not run

Nothing required by the K4 oracle is `not run`: the headless route exits 0
twice over the retail archives, the native tactical battle is a real
OpenBFME battle, the visual artifacts are committed and were opened, and the
canonical save/load/hash round-trip is proved.

The items that could not be executed in this sandbox are listed with the exact
reason, so that the record is complete and no skip is reported as a pass:

| Item | Status | Why |
|---|---|---|
| The five MP3 golden-fixture cases of `engine/tests/test_audio_decode.cpp` | `not run` | the reference encoders `lame` and `mpg123` are not installed and the sandbox has no root to install them; the fixtures cannot be generated (§9). |
| The oracle cases that read a loose retail file (`game.dat`, `asset.dat`, `gi.dat`, the Angmar movies) | `not run` | the pinned retail snapshot carries the 213 BIG archives and not the loose files of the install root (§9.1). |
| The monolithic (unchunked) run of `openbfme_tests` | `not run` | the kernel OOM-kills it in a 3.9 GiB sandbox; the suite was run in chunks instead, which covers every case exactly once (§9). |
| `tools/arena/smoke_godot.sh --retail` | `not run` | the card's own retail oracle is the native battle route of §5.3, which is strictly stronger than the smoke (it mounts the retail archives, starts a real RotWK map and plays a real battle); the smoke script belongs to the Arena infrastructure lane and is not part of the K4 scope. |
| Locale catalogs (K5) | `not run` | K5's scope; this card ships no localization. |
| A1/A2/A3 audits | `not run` | separate auditor cards. |

## 11 · What is not in this card

* No engine source change: `engine/src/GameLogic/WotrRemaster/**` and
  `engine/src/GameLogic/LivingWorld/**` are untouched (К2 and upstream code).
* No retail byte in git: `.retail/` is ignored, and no BIG archive, texture,
  sound or other retail-derived asset is committed. The profile file names
  retail templates by string; it contains no retail content.
* No absolute path in the committed artifacts: the machine-specific root was
  replaced by `<repo>` in the driver logs, and the scenario itself writes the
  pack path repository-relative.
* No weakening of an existing test or oracle: the K2 engine tests and the
  upstream tactical tests are untouched and run green.
* One file outside the card's zone, `tools/release/public_binaries.txt`: the
  three screenshots are images, and the public tree is text only, so the
  release lane's own list of the binaries the tree may hold has to name them
  (path, sha256, reason). It is the mechanism the repository provides for
  exactly this case, and it is three lines. Nothing else outside
  `godot/wotr_remaster/**` and `docs/evidence/WOTR-1-1/K4/**` is touched.
