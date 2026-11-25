# Acceptance Report — WOTR-1.1 · Def-A1-1 (Save Envelope Canonical State Hash)

- **Task ID**: `WOTR-1.1 · Def-A1-1` (`dispatch_id`: `4226f0cf-5705-81b0-95d3-d67194b50886`, `run_id`: `b7aaef16-16dc-492f-ba90-39e653af77a6`)
- **Role**: Strategic Core Engineer (`Executor`)
- **Branch**: `arena/e60ab74a-open-bfme`
- **Base SHA**: `14afddff96d2aa0a14854fd541c2bdfe056f9ad0`
- **Code SHA**: `6ad08a4bbc36ba535fc3a0a0cd5163172bdd93c4`
- **Report SHA**: `docs/evidence/WOTR-1-1/Def-A1-1/report.md` (committed in the report commit immediately atop `6ad08a4bbc36ba535fc3a0a0cd5163172bdd93c4`; see PR head commit)
- **Cost Tier**: `full` (`T2`, `Build: full`, `Retail: none` per `docs/missions/WOTR-1.1.md` §7 and `docs/agents/cost-gates.md`)
- **Date**: `2026-11-25`

---

## 1. Scope & Files Touched

All modified and added files reside strictly within the authorized role zone (`engine/src/GameLogic/WotrRemaster/**`, `engine/tests/WotrRemaster/**`, `docs/evidence/WOTR-1-1/Def-A1-1/**`):

- `engine/src/GameLogic/WotrRemaster/Save.h` — Added `std::string stateHash;` to `WotrSaveEnvelopeV1` and documented the `stateHash == stateHash(parsedState)` cross-check invariant at `loadCampaign`.
- `engine/src/GameLogic/WotrRemaster/Save.cpp` — Included `stateHash` inside `saveEnvelopePayload` (the SHA-256-sealed envelope payload) and `serializeSaveEnvelope`; populated `e.stateHash = stateHash(s)` in `saveCampaign`; required `stateHash` as a string field in `parseSaveEnvelope` (plus raw canonical-envelope SHA-256 mismatch classification when a byte flip lands on a JSON escape sequence); cross-checked `e.stateHash == stateHash(s)` and `canonicalState(s) == e.stateBytes` in `loadCampaign`, refusing mismatches with `WotrErrc::DamagedSaveChecksum` (`ERR_DAMAGED_SAVE_CHECKSUM`).
- `engine/tests/WotrRemaster/test_save.cpp` — Added 4 regression `TEST_CASE`s covering: (1) exact A1 PROBE 2 reproducer (`army_gondor.regionId = east` while a pending battle involving `army_gondor` exists in `crossing`, re-sealed over `serializeSaveEnvelope`), (2) legitimate save/load byte-identity and `e.stateHash == stateHash(s)`, (3) `stateHash` inclusion inside the envelope payload checksum, forged `stateHash` refusal, and non-canonical `stateBytes` refusal even when `stateHash` matches `stateHash(parsedState)`, and (4) missing `stateHash` (`ERR_INVALID_SAVE`) and re-sealed unknown inner `schemaVersion` (`ERR_UNKNOWN_SAVE_VERSION`) without lenient fallback.
- `docs/evidence/WOTR-1-1/Def-A1-1/report.md` — This acceptance report.
- `docs/evidence/WOTR-1-1/Def-A1-1/artifacts/standalone_doctest.txt` — Focused standalone `WotrRemaster` doctest run transcript.
- `docs/evidence/WOTR-1-1/Def-A1-1/artifacts/save_integrity_probe.txt` — Unmodified `docs/evidence/WOTR-1-1/A1/artifacts/a1_probe.cpp` execution output after the fix.
- `docs/evidence/WOTR-1-1/Def-A1-1/artifacts/a1_audit.txt` — Unmodified `docs/evidence/WOTR-1-1/A1/artifacts/a1_audit.cpp` 73-check execution output after the fix.

No files outside the assigned role zone and card `Scope` were modified.

---

## 2. Design & Implementation Summary

### Defect Closed (`Def-A1-1`)
Auditor card `A1` (`docs/evidence/WOTR-1-1/A1/report.md` §3, `Def-A1-1`) demonstrated that `WotrSaveEnvelopeV1` previously sealed `(commandCursor, compatibilityProfile, contractVersion, envelopeVersion, packContentHash, packId, packVersion, stateBytes)` with outer SHA-256 `checksum`, but carried no canonical `stateHash` field and did not verify `stateBytes` canonicality against the parsed `CampaignState`. Consequently, modifying `stateBytes` and calling `serializeSaveEnvelope` (as in `a1_probe.cpp` PROBE 2) bypassed `parseSaveEnvelope` checksum verification without carrying or matching a canonical state digest.

### Solution
1. **Envelope representation & sealing (`Save.h`, `Save.cpp`)**:
   - `WotrSaveEnvelopeV1` now carries `std::string stateHash;` (canonical lowercase SHA-256 hex digest of `canonicalState(s)`).
   - `saveEnvelopePayload(const WotrSaveEnvelopeV1&)` includes `"stateHash"` in the sorted canonical JSON object hashed into `e.checksum`, and `serializeSaveEnvelope` emits `"stateHash"` alongside `"checksum"`.
   - `saveCampaign(const CampaignPack&, const CampaignState&, std::size_t)` sets `e.stateBytes = canonicalState(s)` and `e.stateHash = stateHash(s)`.
2. **Strict envelope parsing (`parseSaveEnvelope`)**:
   - Rejects envelopes lacking a string `"stateHash"` field with `WotrErrc::InvalidSave` (`ERR_INVALID_SAVE`) — no silent migration and no fallback for stale/legacy envelopes missing `stateHash`.
   - When raw byte corruption inside the body of a canonically framed `{"checksum":"<64-lower-hex>",...}` envelope lands on a JSON escape sequence (for example, flipping `\` in `\"members\"` inside `"stateBytes"`), `hasDamagedCanonicalEnvelopeChecksum` reconstructs the raw payload bytes without `"checksum":...,` and verifies SHA-256 so raw byte corruption consistently reports `WotrErrc::DamagedSaveChecksum` (`ERR_DAMAGED_SAVE_CHECKSUM`) rather than collapsing into a generic JSON syntax error.
3. **Canonical state hash & byte cross-check at load (`loadCampaign`)**:
   - After verifying envelope/contract versions (`ERR_UNKNOWN_SAVE_VERSION`), pack/profile compatibility (`ERR_INCOMPATIBLE_PACK`), and parsing `e.stateBytes` via `parseState` (`ERR_UNKNOWN_SAVE_VERSION` on unknown inner `schemaVersion`, `ERR_INVALID_SAVE` on structurally invalid inner JSON), `loadCampaign` computes `actualStateHash = stateHash(parsedState)` and `canonicalBytes = canonicalState(parsedState)`.
   - Requires both `e.stateHash == actualStateHash` and `canonicalBytes == e.stateBytes`; otherwise returns `WotrErrc::DamagedSaveChecksum` (`ERR_DAMAGED_SAVE_CHECKSUM`) with a descriptive message naming expected vs. actual canonical state hashes or non-canonical `stateBytes`.

### Допущение (Autonomous Design Notes)
- **Допущение 1 (Error code selection)**: Published `WotrErrc` numeric codes in `Errors.h` are frozen (`Never renumber existing codes once published`). `WotrErrc::MismatchedChecksum` (`13`) is documented specifically for `BattleResult.requestChecksum != pending request` in the battle contract, whereas `WotrErrc::DamagedSaveChecksum` (`17`, `ERR_DAMAGED_SAVE_CHECKSUM`) is the dedicated save-envelope integrity failure code. Returning `WotrErrc::DamagedSaveChecksum` when `e.stateHash != stateHash(parsedState)` or `canonicalState(parsedState) != e.stateBytes` preserves frozen `WotrErrc` numbering, keeps `parseState` version/schema errors (`ERR_UNKNOWN_SAVE_VERSION` on `a1_audit` check N4 and `ERR_INVALID_SAVE` on PROBE 1) intact by running after `parseState`, and requires zero changes to `godot/wotr_remaster/core/native/` (which maps any `WotrError` generically via `errcName(res.error().code)`).
- **Допущение 2 (Envelope version)**: Per card specification (`WotrSaveEnvelopeV1 carries stateBytes and commandCursor... Add a stateHash field hashed inside the envelope payload`), `kSaveEnvelopeVersion` remains `1` (`WOTR-1.1` has not shipped persistent user saves yet), and envelopes missing `stateHash` fail hard with `ERR_INVALID_SAVE` with no silent fallback.

---

## 3. Verification & Evidence

### 3.1 Focused Standalone Build & `WotrRemaster` Doctest Suite
Command:
```bash
g++ -std=c++17 -O2 -Wall -Wextra -I engine/src -I engine/thirdparty/doctest \
  -DWOTR_FIXTURE_DIR=campaigns/fixtures/wotr_1_1 \
  engine/src/GameLogic/WotrRemaster/*.cpp engine/tests/WotrRemaster/*.cpp \
  -o /tmp/wotr-def-a1-1/wotr_tests && \
/tmp/wotr-def-a1-1/wotr_tests
```
- **Exit code**: `rc=0`
- **Result**: `36 test cases: 36 passed | 0 failed | 0 skipped`, `332 assertions: 332 passed | 0 failed`
- **Transcript**: `docs/evidence/WOTR-1-1/Def-A1-1/artifacts/standalone_doctest.txt` (`sha256=4fc6aa6dca055e986b34646f00e38bf4354317368aa365daf0799eca92769340`)

### 3.2 Unmodified Auditor Save-Integrity Reproducer (`a1_probe.cpp`)
Command:
```bash
g++ -std=c++17 -O2 -Wall -Wextra -I engine/src \
  docs/evidence/WOTR-1-1/A1/artifacts/a1_probe.cpp engine/src/GameLogic/WotrRemaster/*.cpp \
  -o /tmp/wotr-def-a1-1/a1_probe && \
/tmp/wotr-def-a1-1/a1_probe campaigns/fixtures/wotr_1_1
```
- **Exit code**: `rc=0`
- **Output** (`docs/evidence/WOTR-1-1/Def-A1-1/artifacts/save_integrity_probe.txt`, `sha256=fa7a848ea29e2f0e213ee0403dab690eed36e6b089a1c3587d86e8625f36f1a1`):
```text
BASELINE state_bytes_len=1812 state_hash=033cae70104415a3194ead818c87aad502ad3cf96c81f15021e5966df18e2887
PROBE1 dropped_armies_and_regions accepted=false err=ERR_INVALID_SAVE loaded_state_len=- loaded_state_hash=- equals_original=-
PROBE2 teleported_army accepted=false err=ERR_DAMAGED_SAVE_CHECKSUM loaded_army_region=-
PROBE3 forged_command_checksums accepted=false err=ERR_DAMAGED_SAVE_CHECKSUM
PROBE4 forged_pending_request_checksum accepted=false err=ERR_DAMAGED_SAVE_CHECKSUM
PROBE5 legit_load ok=true bytes_equal=true
```

### 3.3 Unmodified Auditor Determinism & Contract Harness (`a1_audit.cpp`)
Command:
```bash
g++ -std=c++17 -O2 -Wall -Wextra -I engine/src \
  docs/evidence/WOTR-1-1/A1/artifacts/a1_audit.cpp engine/src/GameLogic/WotrRemaster/*.cpp \
  -o /tmp/wotr-def-a1-1/a1_audit && \
/tmp/wotr-def-a1-1/a1_audit campaigns/fixtures/wotr_1_1
```
- **Exit code**: `rc=0`
- **Summary**: `AUDIT_SUMMARY checks=73 failures=0`
- **Transcript**: `docs/evidence/WOTR-1-1/Def-A1-1/artifacts/a1_audit.txt` (`sha256=d95eea4a575de45904ef75319b470d92b682ea41425f7ec5a9acd00082c10239`)
- **K2 Determinism Invariant Verification** — all 5 accepted fixture boundary canonical lengths and SHA-256 state hashes are byte-for-byte identical to `docs/evidence/WOTR-1-1/A1/report.md` §4:

| Boundary | Canonical Bytes | `stateHash` (SHA-256) | Matches Baseline |
|---|---:|---|---|
| `initial_state` (seed `20261121`) | `1049` | `29689fc52edb46a45a3960c641c9fe18803a4e9c59c54c8167d3b4b61ac28c73` | Yes |
| `after_resolve` | `1829` | `ad8b96a15277817e9a00b1c44d93c6eb7963efd281336d972a1665e6632ea682` | Yes |
| `after_apply` | `1455` | `1c0c0065cf8602ab18aac5dbc801904e43b2051fda7e06ae76817381d5f127f0` | Yes |
| `after_load_fresh_instance` | `1455` | `1c0c0065cf8602ab18aac5dbc801904e43b2051fda7e06ae76817381d5f127f0` | Yes |
| `next_step_from_loaded` | `1749` | `b78fb7c162c2b9dbf5edfd4aff90ac5145b9b84171ce3f254f944d2a9da9442a` | Yes |

- Save envelope with `stateHash`: `envelope_len=2045`, `sha256=4eaac650fa890ddea36a34ae1eca9abc199cc8c0963724051ec99533e4d4d0dd`, `state_bytes_eq_canonical=true`.

### 3.4 Full Cost Gate (`T2`: `Build: full`, `Retail: none`)
Executed once on committed `Code SHA` `6ad08a4bbc36ba535fc3a0a0cd5163172bdd93c4`:

1. **Runtime bootstrap**:
   - Command: `bash tools/arena/bootstrap.sh --runtime`
   - Exit code: `rc=0` (CMake `4.4.5`, Ninja `1.13.2`, Godot `4.7.2.stable.custom_build.ed1daf0bf` + SwiftShader hydrated and verified).
2. **Audio fixture hydration (`cost-gates.md` §5.1)**:
   - Unpacked `wotr-audio-fixtures.tar.gz` (`sha256=1e58106afd3ebf8f535b8aa11a040a29665051b0266caddc1984ccdfdf6ea052`, 46,386 bytes) from `Arena/wotr-remaster/artifacts` into `/tmp/wotr-audio-fixtures`.
   - Command: `python3 tools/audio/gen_mp3_fixtures.py --check-only --out /tmp/wotr-audio-fixtures`
   - Exit code: `rc=0` (`Verified 17 MP3 fixtures (sizes and SHA-256)`).
3. **Full engine + GDExtension + unit test build**:
   - Command: `bash tools/arena/build_engine.sh --tests`
   - Exit code: `rc=0` (`2618/2618` targets built; `engine/build/openbfme_tests` `sha256=9d30b35bc8fe533156263239eeb6b380807d8eea535fd104045d3c176de2eca7`; `godot/bin/openbfme.linux.template_debug.x86_64.so` `sha256=5f32b2608bf875c307076be3d7ec0f6f95ca62691603c087c6b8c2b1955264a4`).
4. **Full engine test suite (`openbfme_tests`)**:
   - Command: `OPENBFME_AUDIO_FIXTURES=/tmp/wotr-audio-fixtures ./engine/build/openbfme_tests`
   - Exit code: `rc=0`
   - Result: `5652 test cases: 5652 passed | 0 failed | 27 skipped` (all 27 skips are `ROTWK_INSTALL / BFME2_INSTALL` retail-only tests under `Retail: none`; all 17 MP3 audio fixture tests executed and passed), `1,410,254 assertions: 1,410,254 passed | 0 failed`.
5. **Godot headless smoke & native `WotrCorePort` campaign scenario**:
   - Command: `bash tools/arena/smoke_godot.sh`
   - Exit code: `rc=0` (`SMOKE PASS: extension and scripts loaded; retail intentionally absent`).
   - Command: `PATH="/tmp/buildenv/bin:$PATH" bash godot/wotr_remaster/core/native/build_core_extension.sh && GODOT_BIN=/tmp/godotbin/godot-swiftshader bash godot/wotr_remaster/scenario/run_scenario.sh`
   - Exit code: `rc=0` (`godot/wotr_remaster/core/native/bin/libwotr_core.so` `sha256=2a059fa7f3f8c85d09103ae423314f2a48562340cecab67cc20289aa1537da21`; `WOTR-K4 PASS`, native save envelope `2252` bytes `sha256=0d1c4f2aa45cb5893ccd34b67e0d174575667989943dc6d585dd76efaf2b4006`, fresh-session reload `bytes_equal=true`, `state_hash=5ab6958ba0ed8d32985e2e4b2092d6e4dbc3a790d6daf3ad134b8a974581e32b`). Untracked Godot `.import`/`.uid` cache files created during scenario execution were cleaned immediately so `godot/` remains untouched.

---

## 4. Acceptance Criteria Check

| Requirement / Oracle / Invariant | Status | Evidence |
|---|---|---|
| `WotrSaveEnvelopeV1` carries `stateHash` hashed inside envelope payload and `loadCampaign` requires `stateHash == stateHash(parsedState)` | PASS | `Save.h`, `Save.cpp` (`saveEnvelopePayload`, `serializeSaveEnvelope`, `saveCampaign`, `parseSaveEnvelope`, `loadCampaign`) |
| Focused standalone build and doctest suite (`g++ -std=c++17 -O2 -Wall -Wextra`, `-DWOTR_FIXTURE_DIR=campaigns/fixtures/wotr_1_1`) exit 0 | PASS | `rc=0`, `36/36` test cases (`332` assertions) passed (`artifacts/standalone_doctest.txt`) |
| New regression test refuses A1 PROBE 2 reproducer (`army_gondor.regionId = east` while pending battle in `crossing`) with named error | PASS | `test_save.cpp` (`save: A1 PROBE 2 re-sealed envelope over altered inner state is refused`) + unmodified `a1_probe.cpp` (`PROBE2 teleported_army accepted=false err=ERR_DAMAGED_SAVE_CHECKSUM`) |
| Invariant 1: §8 K2 determinism invariants unchanged (canonical state bytes & hashes of accepted fixture do not move) | PASS | `a1_audit.cpp` `73/73` passed; all 5 boundary hashes (`29689fc5...`, `ad8b96a1...`, `1c0c0065...`, `1c0c0065...`, `b78fb7c1...`) unchanged (`artifacts/a1_audit.txt`) |
| Invariant 2: Legitimately produced save round-trips byte-identically | PASS | `test_save.cpp`, `a1_probe` (`PROBE5 legit_load ok=true bytes_equal=true`), `a1_audit`, and Godot `WotrCorePort` scenario (`bytes_equal=true`) |
| Invariant 3: Unknown or stale envelope/contract/inner schema versions fail with named error; no silent migration or lenient fallback | PASS | `ERR_UNKNOWN_SAVE_VERSION` on unknown `envelopeVersion`, `contractVersion`, or inner `schemaVersion`; `ERR_INVALID_SAVE` when `stateHash` is absent |
| Invariant 4: Writes strictly inside role zone / `Scope`; oracle not weakened | PASS | Only `engine/src/GameLogic/WotrRemaster/Save.{h,cpp}`, `engine/tests/WotrRemaster/test_save.cpp`, and `docs/evidence/WOTR-1-1/Def-A1-1/**` touched |
| Full gate (`Build: full`, `Retail: none`) run once before handoff | PASS | `bootstrap.sh --runtime` (`rc=0`), `build_engine.sh --tests` (`rc=0`), `openbfme_tests` (`5652/5652` passed, `rc=0`), `smoke_godot.sh` (`rc=0`) |

---

## 5. Known Limitations / Risks / Follow-ups

- **Relation to sibling cards `Def-A1-2` and `Def-A1-3`**: `Def-A1-1` closes save-envelope state-hash sealing and canonical cross-checking (`e.stateHash == stateHash(parsedState)` and `canonicalState(parsedState) == e.stateBytes`), so any modification of `stateBytes` re-sealed without recomputing `e.stateHash` (including unmodified `a1_probe` PROBE 2, PROBE 3, and PROBE 4) is refused at `loadCampaign` with `ERR_DAMAGED_SAVE_CHECKSUM`. If an adversary constructs a semantically inconsistent `CampaignState` or forges inner `CommandEnvelope.checksum` / `BattleStartRequest.checksum` values *and* recomputes both `e.stateHash = sha256(canonicalState)` and outer `e.checksum`, rejecting that state belongs to `parseState` semantic validation (`Def-A1-2`) and inner cryptographic checksum verification (`Def-A1-3`), which compose cleanly with `Def-A1-1` because `loadCampaign` invokes `parseState(e.stateBytes)` before returning.

---

## 6. Artifacts Index

| Path | Size (bytes) | SHA-256 |
|---|---:|---|
| `engine/src/GameLogic/WotrRemaster/Save.h` | `2319` | `c4d9faa5b5d7bcdd882909fc1522cf109e4a1ae63fabe7c157dfacb32d8163a1` |
| `engine/src/GameLogic/WotrRemaster/Save.cpp` | `26956` | `3f969ffde2e281f215bbaddc739234002fe815fc4814e98b04fe41994b7beee4` |
| `engine/tests/WotrRemaster/test_save.cpp` | `13356` | `77ffda05423ec75a7fe78bdb10734418ab006383b30ef0efa50538521714d0c0` |
| `docs/evidence/WOTR-1-1/Def-A1-1/artifacts/standalone_doctest.txt` | `299` | `4fc6aa6dca055e986b34646f00e38bf4354317368aa365daf0799eca92769340` |
| `docs/evidence/WOTR-1-1/Def-A1-1/artifacts/save_integrity_probe.txt` | `526` | `fa7a848ea29e2f0e213ee0403dab690eed36e6b089a1c3587d86e8625f36f1a1` |
| `docs/evidence/WOTR-1-1/Def-A1-1/artifacts/a1_audit.txt` | `4986` | `d95eea4a575de45904ef75319b470d92b682ea41425f7ec5a9acd00082c10239` |
