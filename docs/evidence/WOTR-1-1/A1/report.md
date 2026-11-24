# WOTR-1.1 · A1 · Determinism & save audit (Auditor)

**Card:** `WOTR-1.1 · A1 · Determinism & save audit` · Notion page `41f6f0cf-5705-81a8-984d-cda5dc9f96ee` (Sequence 6)
**Role:** Determinism & Save Auditor · **Zone:** `docs/evidence/WOTR-1-1/A1/**`
**Date:** 2026-11-24 · **Accepted clean SHA under audit:** `519d82eb926cb641e1c4b2ce6466868524e79c93`
**Toolchain:** `g++ (Debian 12.2.0-14+deb12u1) 12.2.0`, doctest 2.4.12 (vendored), Linux x86_64

## 1 · Verdict

**Oracle `§8 A1`: PASS.** All four clauses were reproduced independently, from a clean checkout of the accepted SHA, without reusing any executor-generated pass file.

| `§8 A1` clause | Result |
|---|---|
| Reruns from a clean checkout on the accepted SHA with **separate output directories** | PASS — two independent clones of `519d82e`, output dirs `/tmp/wotr-a1/run1`, `/tmp/wotr-a1/run2` |
| **At least two complete runs** have equal command log, state bytes and hash | PASS — 5 runs, boundary log SHA-256 `7be1dc34…e15dc` in all five |
| **Corrupt, unknown-version and mismatched-pack** inputs fail **before** state mutation | PASS — 12 negative cases, every one leaves the protected state byte-identical |
| Auditor does **not reuse executor-generated pass files** as proof | PASS — every number below comes from the auditor's own harness (`artifacts/a1_audit.cpp`), built by the auditor |

The audit also opened a **second, independent defect file** (§7) for save-envelope integrity. Those findings do **not** falsify the A1 oracle as written, and they are **not** a verdict on K2/K4 acceptance: they are named to their owner for a separate card.

Cost gate as declared: `build: focused`, `retail: none`. Executed level **T1 focused** — one target (`WotrRemaster` standalone), named tests plus the auditor harness. No full engine build, no retail hydration; `T2`/`T3` were **not run** and are not required by this card.

## 2 · Provenance of the audited tree

| Fact | Value |
|---|---|
| Accepted SHA | `519d82eb926cb641e1c4b2ce6466868524e79c93` (merge of PR #37) |
| Parent of accepted SHA | `8dd2b71` |
| Current branch base | `f4c52e9424a90d92ab4867963db22ffac4ce4e59` |
| Difference `519d82e..f4c52e9` | two files, 2 commits behind (`gh api …/compare/519d82e...f4c52e9` → `ahead_by: 2`): `docs/L10N.md` and `docs/missions/WOTR-1.1.md`. Neither touches `engine/src/GameLogic/WotrRemaster/**`, `engine/tests/WotrRemaster/**` or `campaigns/fixtures/wotr_1_1/**`, so the audited code and fixture are unchanged. (An earlier revision of this row said "one file: `docs/L10N.md`"; that was wrong — corrected 2026-11-24.) |
| Clones | `git init` + `git fetch --depth 1 origin 519d82e` into `/tmp/wotr-a1/co1` and `/tmp/wotr-a1/co2` |
| Clone working trees | `diff -r --exclude=.git co1 co2` → no differences; both `git status --porcelain` → 0 lines |
| Fixture | `campaigns/fixtures/wotr_1_1/pack.json`, 1069 bytes, SHA-256 `e78e903e6dad8ed45820bbb8f9f8221196c1535b049dfa315191403194028878` |

The build does **not** run CMake: at this SHA the `WotrRemaster` lane is still not registered in `engine/CMakeLists.txt` or `engine/cmake/*.cmake` (`grep -rn WotrRemaster engine/CMakeLists.txt engine/cmake/*.cmake` → no output), and `cmake`/`ninja` are absent from the audit sandbox. The card's own command line is the standalone `g++` build, which is what was executed.

## 3 · Commands and return codes

### 3.1 Fixture pack hash and the auditor harness

```bash
# run 1 (checkout co1)
g++ -std=c++17 -O2 -Wall -Wextra -I co1/engine/src \
    -DWOTR_FIXTURE_DIR=$PWD/co1/campaigns/fixtures/wotr_1_1 \
    a1_audit.cpp co1/engine/src/GameLogic/WotrRemaster/*.cpp -o run1/a1_audit      # rc 0, 3 unused-function warnings
# run 2 (checkout co2)
g++ -std=c++17 -O2 -Wall -Wextra -I co2/engine/src \
    -DWOTR_FIXTURE_DIR=$PWD/co2/campaigns/fixtures/wotr_1_1 \
    a1_audit.cpp co2/engine/src/GameLogic/WotrRemaster/*.cpp -o run2/a1_audit      # rc 0, 3 unused-function warnings
```

Both binaries are byte-identical: SHA-256 `d26fa32a28d9e33e0559c5bf241126d6d6850678addb42bb6253a1910596a7b8`.
The warnings are the three pre-existing `defined but not used` helpers in `Save.cpp:26`, `Save.cpp:32`, `State.cpp:53`; they are reported as facts, not as failures (see §8, note 1).

### 3.2 The two oracle runs

```bash
cd /tmp/wotr-a1/run1 && ./a1_audit /tmp/wotr-a1/co1/campaigns/fixtures/wotr_1_1   # rc 0
cd /tmp/wotr-a1/run2 && ./a1_audit /tmp/wotr-a1/co2/campaigns/fixtures/wotr_1_1   # rc 0
```

`AUDIT_SUMMARY checks=73 failures=0` in both runs. Logs are committed at
[`artifacts/boundaries_run1.txt`](artifacts/boundaries_run1.txt) and
[`artifacts/boundaries_run2.txt`](artifacts/boundaries_run2.txt); they are **byte-identical**
(SHA-256 `7be1dc342b6f4988689fe1302d7cebb10b6141ecbd232388378b3eef125e15dc`).

### 3.3 The executor's own suite, rebuilt by the auditor

```bash
g++ -std=c++17 -O2 -Wall -Wextra -I co1/engine/src -I co1/engine/thirdparty/doctest \
    -DWOTR_FIXTURE_DIR=$PWD/co1/campaigns/fixtures/wotr_1_1 \
    co1/engine/src/GameLogic/WotrRemaster/*.cpp co1/engine/tests/WotrRemaster/*.cpp \
    -o run1/wotr_tests                                                             # rc 0
cd run1 && ./wotr_tests                                                            # rc 0
```

```text
[doctest] test cases:  32 |  32 passed | 0 failed | 0 skipped
[doctest] assertions: 273 | 273 passed | 0 failed |
[doctest] Status: SUCCESS!
```

Identical result in `run2` ([`artifacts/doctest_run1.txt`](artifacts/doctest_run1.txt),
[`artifacts/doctest_run2.txt`](artifacts/doctest_run2.txt)). The suite is reported as **a rebuild of the
tracked tests by the auditor**, never as an executor pass file: both translation units were compiled from the
clean checkout in this session.

### 3.4 Robustness runs beyond the oracle letter

| Run | Condition differed | Log | Result |
|---|---|---|---|
| 3 | different cwd (`altout/`), `env -i`, `TZ=Pacific/Kiritimati`, `LANG=ru_RU.UTF-8`, `LC_ALL=C.UTF-8`, fixture read from the **other** checkout | [`boundaries_run3_hostile_env.txt`](artifacts/boundaries_run3_hostile_env.txt) | byte-identical |
| 4 | rebuilt `-O0` | [`boundaries_run4_O0.txt`](artifacts/boundaries_run4_O0.txt) | byte-identical |
| 5 | rebuilt `-O3` | [`boundaries_run5_O3.txt`](artifacts/boundaries_run5_O3.txt) | byte-identical |

Hostile `TZ`/locale/environment did not move a single byte, which is the observable form of INV-3 for this core
phase; it is evidence, not a proof of absence (§8).

## 4 · State bytes and hashes at every boundary

Every line below is a line of the auditor's own log (run 1 = run 2 = run 3 = run 4 = run 5).
`bytes_sha256` is SHA-256 of `canonicalState()`; `state_hash` is the core's own `stateHash()`.

| # | Boundary | `len` | `bytes_sha256` = `state_hash` |
|---|---|---|---|
| 1 | `initial_state` (seed `20261121`) | 1049 | `29689fc52edb46a45a3960c641c9fe18803a4e9c59c54c8167d3b4b61ac28c73` |
| 2 | `after_resolve` (two moves → conflict) | 1829 | `ad8b96a15277817e9a00b1c44d93c6eb7963efd281336d972a1665e6632ea682` |
| 3 | `after_apply` (valid `BattleResultV1`) | 1455 | `1c0c0065cf8602ab18aac5dbc801904e43b2051fda7e06ae76817381d5f127f0` |
| 4 | `after_load_fresh_instance` | 1455 | `1c0c0065cf8602ab18aac5dbc801904e43b2051fda7e06ae76817381d5f127f0` |
| 5 | `next_step_from_loaded` (turn 2 move) | 1749 | `b78fb7c162c2b9dbf5edfd4aff90ac5145b9b84171ce3f254f944d2a9da9442a` |

Independent facts recorded on the same run:

- **Pack identity.** `contentHashOf(pack)` = `e1835095e2a93d46582ac182ec625c1f4c5b4b6902bb45ca26212ccff5d4dd74`, equal to the manifest's declared `contentHash`; canonical pack bytes are 829 bytes.
- **Seed is an input, not ambient.** seed `20261121` → `29689fc5…8c73`; seed `20261122` → `e918d24a2208ddeb9abcdd8f1a86c20335552b8fafd99e264362dafd82dfaa2b`. The explicit seed changes the canonical state; `initialSeed` is carried in the bytes.
- **Submission order is irrelevant.** Process A submitted `{gondor, mordor}`, process B `{mordor, gondor}`; after resolution both are `ad8b96a1…a682` with the same battle id.
- **Battle request.** `battle:crossing:0`, 2 participants, `requestChecksum` `72a2861f56372196609b99079a65e3bcb3b5fd05024fbb928fa41682fe6fead7`, self-consistent (`battleRequestChecksum(request) == requestChecksum`), `tacticalSeed = 20261121`, request carries `stateHash` `e5bc3fa15254778091e13c828cdf7a420b231fffde451163db0511f615e17b00`.
- **Result.** checksum `c35af7eba4119190404edfbf74be4939d48a1542825a9544001e145d9f3b32cf`; apply advances turn 0→1, returns the phase to `Planning`, gives `crossing` to `faction_gondor`, removes `army_mordor`, clears the pending battle.
- **Save envelope.** 1966 bytes, SHA-256 `c40a460950dcb43be21306c5920124afeef90681990b20ca7af7e207570364b0`, saved twice → identical bytes. `envelopeVersion=1`, `contractVersion=1`, `commandCursor=2`, `rngMetadata="{}"`, embedded `stateBytes` SHA-256 `1c0c0065…27f0` and **equal to the canonical state verbatim**, `packHash` equal to the pack content hash, `tacticalProfile=vanilla_rotwk_201`.
- **Fresh-instance load.** `loadCampaign` reproduces boundary 4 exactly; the loaded instance and the in-memory instance then produce the same next-step boundary 5.
- **Source property order.** A pack with reversed `regions`/`factions`/`armies` arrays and reversed `connections` arrays hashes to the same canonical pack hash `e1835095…dd74` and builds the same initial-state bytes.
- **No ambient input in the core.** `g++ -Wall -Wextra` is clean apart from the three unused helpers; the core's 6 `.cpp`/`.h` pairs contain no `chrono`/`clock`/`time()`/`localtime`/`gmtime`/`rand`/`random_device`/`getenv`/`setlocale`/`std::locale`/`filesystem`/`getpid`/`thread`/`mutex`/Godot reference (one occurrence of the word "Godot" is a comment in `State.h:7`). `std::unordered_set` appears five times in `Pack.cpp`, all as membership sets that are never iterated; every order that reaches bytes is sorted.

## 5 · Negative cases: fail before state mutation

Each case records the named error, the message's SHA-256 (the message itself is context, not proof) and then
asserts that the protected state is **byte-identical** afterwards.

### 5.1 Command and battle-result boundary

| Case | Named error | State unchanged |
|---|---|---|
| command with damaged envelope checksum | `ERR_DAMAGED_CHECKSUM` | yes |
| command for unknown army | `ERR_UNKNOWN_ENTITY` | yes |
| non-adjacent move | `ERR_INVALID_COMMAND` | yes |
| command by a player who does not own the army | `ERR_INVALID_COMMAND` | yes |
| the same army twice in one batch | `ERR_INVALID_COMMAND` | yes |
| result with a wrong battle id | `ERR_UNKNOWN_BATTLE` | yes |
| result with a mismatched request checksum | `ERR_MISMATCHED_CHECKSUM` | yes |
| result with an unknown contract version | `ERR_INVALID_RESULT` | yes |
| result with a damaged result checksum | `ERR_DAMAGED_CHECKSUM` | yes |
| result naming an unknown surviving member | `ERR_UNKNOWN_ENTITY` | yes |
| **second apply** of the same valid result | `ERR_UNKNOWN_BATTLE` | yes |

### 5.2 Save/load boundary

| Case | Named error | State unchanged |
|---|---|---|
| envelope with a flipped byte inside `stateBytes` | `ERR_DAMAGED_SAVE_CHECKSUM` | yes |
| `envelopeVersion = 2` | `ERR_UNKNOWN_SAVE_VERSION` | yes |
| `contractVersion = 9` | `ERR_UNKNOWN_SAVE_VERSION` | yes |
| **validly re-sealed** envelope whose embedded state has `schemaVersion = 2` | `ERR_UNKNOWN_SAVE_VERSION` | yes |
| save loaded against a pack with a **different id** | `ERR_INCOMPATIBLE_PACK` | yes |
| save loaded against a pack with **different content** (same id, `army_gondor` quantity 13) | `ERR_INCOMPATIBLE_PACK` | yes |
| pack whose declared `contentHash` is damaged | `ERR_CONTENT_HASH_MISMATCH` (pack rejected; no state built) | n/a |

The "re-sealed" rows matter: the envelope checksum was recomputed by the auditor, so the refusal comes from the
inner-state validator, not from the envelope checksum. That is the clause "unknown-version … fails before state
mutation" tested where it actually lives.

## 6 · What was **not** run

| Item | Status | Reason |
|---|---|---|
| `T2` full engine build (`tools/arena/build_engine.sh --tests`) | **not run** | Card declares `build: focused`; `cmake`/`ninja` absent; the WotrRemaster lane has no CMake registration at this SHA |
| `T3` retail | **not run** | Card declares `retail: none`; no retail criterion in `§8 A1` |
| `openbfme_tests` full suite | **not run** | Not required by the card; no CMake in the sandbox |
| Godot headless route | **not run** | Belongs to K4; the auditor does not re-run another card's oracle as its own proof |
| MSan/ASan, `-fsanitize=undefined` | **not run** | Out of the declared gate; any finding would be a separate card |

## 7 · Defects found by this audit (new cards, not part of the A1 verdict)

The core's `Save.cpp` deliberately checks the **envelope** checksum only. `WotrSaveEnvelopeV1` carries
`stateBytes` and a `commandCursor`, but **no separate canonical state hash**, and `loadCampaign()` does not
cross-check the embedded state's fields beyond structural validity and referential integrity. The auditor tested
what that permits by **re-sealing** envelopes with a correct envelope checksum over a modified inner state.

| ID | Severity | Reproducer | Expected | Actual |
|---|---|---|---|---|
| **Def-A1-1** | high | `a1_probe` PROBE 2 — re-seal with `army_gondor.regionId = "east"` (it is in `crossing`, `east` is not adjacent) | load refuses with a named error | `accepted=true err=Ok loaded_army_region=east` |
| **Def-A1-2** | high | `a1_probe` PROBE 3 — re-seal with every `acceptedCommands[*].checksum` replaced by `ffff…f` | load refuses (`ERR_DAMAGED_CHECKSUM`) | `accepted=true`; and a re-save keeps the forged log (`still_forged`) — a forged command history becomes durable truth |
| **Def-A1-3** | medium | `a1_probe` PROBE 4 — re-seal with `pendingBattle.requestChecksum` replaced by `0000…0` | load refuses: the pending request can no longer be reproduced | `accepted=true`; afterwards a legitimate result sealed against the **real** request is rejected with `ERR_MISMATCHED_CHECKSUM`, i.e. the forged save permanently wedges that battle |

Fact that limits the blast radius: PROBE 1 (re-seal with the `armies` and `regions` arrays emptied) **is**
rejected with `ERR_INVALID_SAVE` — referential integrity does hold for dangling references. The three defects
above are about values that are structurally valid and therefore pass.

Smallest fix directions (owner decides; the auditor does not repair):

1. add a `stateHash` field to `WotrSaveEnvelopeV1` (hashed as part of the envelope payload) and require
   `stateHash == stateHash(parsedState)` at load;
2. on load, verify every `acceptedCommands[*].checksum` with `envelopeChecksum()`;
3. on load, require `pendingBattle` ⟺ `phase == BattlePending` and that `commandCursor` equals
   `acceptedCommands.size()`.

Each of 1–3 would have failed at least one probe. All three are pure core changes inside the K2 zone.

**Also reported as a fact, not a failure:** `docs/agents/pr-rules.md` Р10 names
`node tools/script-cat/preflight.mjs` (the paragraph itself says the path lives in `relay-wotr`), and the line is
stale enough to mislead a role that reads only the code block.

## 8 · Limits of this audit

1. **Warnings are real.** `-Wall -Wextra` reports three `defined but not used` helpers (`Save.cpp:26`,
   `Save.cpp:32`, `State.cpp:53`). They do not affect behaviour; they are named so that a later cleanup card can
   delete them rather than rediscover them.
2. **"No ambient input" is observed, not proved.** The hostile-environment run plus a source scan are strong
   empirical evidence, not a formal proof: a reference could be hidden behind a macro, an inlined call or a
   library default. A complete proof needs a build with `-ffreestanding`-style restrictions or an MSan run; both
   are `not run` (§6).
3. **One platform, one compiler.** All runs are `g++ 12.2.0` on Linux x86_64. Equality across libstdc++ versions,
   on MSVC/clang, or on a big-endian target is **not** established here. The core is written in terms of
   `std::map`/`std::set`/sorted vectors, so the risk is low, but it is outside this card.
4. **Three save-envelope defects, plus one documentation defect.** Probing revealed the save-envelope hardening
   gaps **Def-A1-1..3** (§7) — three defects, not four. The fourth item named in this section was a stale line in
   `docs/agents/pr-rules.md` Р10 (`node tools/script-cat/preflight.mjs`), which is a documentation defect in the
   transport role's zone and is issued separately, not as a save defect. None of the four is silently folded into
   an A1 pass and none is used to fail a card the card text does not cover.
5. **The auditor harness is committed under its own zone.** `artifacts/a1_audit.cpp` and
   `artifacts/a1_probe.cpp` live in `docs/evidence/WOTR-1-1/A1/**`, as the protocol requires the report to live
   under the card's evidence path. They are not wired into the build and are not tests of the product.

## 9 · Artifact index

| Path | SHA-256 |
|---|---|
| `artifacts/a1_audit.cpp` | `1fd7261be0e668664a974247d0d8d2c9aa060241a24c0edf9bf902945123807a` |
| `artifacts/a1_probe.cpp` | `5424b6d8ae072cd0325981002010638e3a002da1e6f2bbf84b0116408ac0dd6a` |
| `artifacts/boundaries_run1.txt` | `7be1dc342b6f4988689fe1302d7cebb10b6141ecbd232388378b3eef125e15dc` |
| `artifacts/boundaries_run2.txt` | `7be1dc342b6f4988689fe1302d7cebb10b6141ecbd232388378b3eef125e15dc` |
| `artifacts/boundaries_run3_hostile_env.txt` | `7be1dc342b6f4988689fe1302d7cebb10b6141ecbd232388378b3eef125e15dc` |
| `artifacts/boundaries_run4_O0.txt` | `7be1dc342b6f4988689fe1302d7cebb10b6141ecbd232388378b3eef125e15dc` |
| `artifacts/boundaries_run5_O3.txt` | `7be1dc342b6f4988689fe1302d7cebb10b6141ecbd232388378b3eef125e15dc` |
| `artifacts/doctest_run1.txt` | `21792fc7eb7bf6fd80aebb11997101130c1f49d01df6e15d3a41a4b3312b4f4f` |
| `artifacts/doctest_run2.txt` | `21792fc7eb7bf6fd80aebb11997101130c1f49d01df6e15d3a41a4b3312b4f4f` |
| `artifacts/build_run1.stderr.txt` | `620cfd5e1d9086909a7476395bf857c4473629280c64663f79973ad4077561cc` |
| `artifacts/save_integrity_probe.txt` | `7d50adda9f1b00cfa7a2ce45e1af790a5133dd4b1b2797b1c41ec0a3291b3796` |
| `artifacts/SHA256SUMS` | self-describing list of the above |
