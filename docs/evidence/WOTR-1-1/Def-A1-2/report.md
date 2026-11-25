# Acceptance Report — WOTR-1.1 · Def-A1-2 (`loadCampaign` Re-verifies `acceptedCommands[*].checksum`)

- **Task ID**: `WOTR-1.1 · Def-A1-2` (`dispatch_id`: `4226f0cf-5705-8141-9bbf-eac612f1df04`, `run_id`: `b893fccb-406c-4083-bac8-b1580ffdebc8`)
- **Role**: Strategic Core Engineer (`Executor`, `41e6f0cf-5705-81a0-904f-e804bb35f4ee`)
- **Branch**: `arena/e60ab74a-open-bfme`
- **Base SHA (`origin/main` & `merge-base HEAD origin/main`)**: `f3eec31f8f8cbb62d6cf500d9348eaf47bd8b20e`
- **Code SHA**: `025da89cb57cb11d97671da935b2aa91ba3fb257`
- **Report SHA**: `docs/evidence/WOTR-1-1/Def-A1-2/report.md` (committed in the report commit immediately atop `025da89cb57cb11d97671da935b2aa91ba3fb257`; see PR head commit)
- **Cost Tier**: `focused` (`T1`, `Build: focused`, `Retail: none` per `docs/missions/WOTR-1.1.md` §7 and `WOTR-D-044`)
- **Date**: `2026-11-25`

---

## 1. Scope & Files Touched

All modified and added files reside strictly within the card `Scope` and the Strategic Core Engineer write zone (`engine/src/GameLogic/WotrRemaster/**`, `engine/tests/WotrRemaster/**`, `docs/evidence/WOTR-1-1/Def-A1-2/**`):

- `engine/src/GameLogic/WotrRemaster/Save.h` — Documented `ERR_DAMAGED_CHECKSUM` on `loadCampaign` when an embedded `acceptedCommands[*].checksum` fails `envelopeChecksum()` verification; `WotrSaveEnvelopeV1` shape and version numbers are unchanged.
- `engine/src/GameLogic/WotrRemaster/Save.cpp` — In `loadCampaign`, validated `acceptedCommands[*].schemaVersion == 1` and `payload.type == CommandType::MoveArmy` (`ERR_INVALID_SAVE` otherwise), eliminated variable shadowing of outer `const WotrSaveEnvelopeV1& e`, and verified `cmd.checksum == envelopeChecksum(cmd)` for every accepted command, refusing mismatches before returning or mutating caller state with `WotrErrc::DamagedChecksum` (`ERR_DAMAGED_CHECKSUM`).
- `engine/tests/WotrRemaster/test_save.cpp` — Added 2 regression `TEST_CASE`s covering: (1) exact A1 PROBE 3 reproducer (`acceptedCommands[*].checksum` replaced by 64 `'f'` chars and outer envelope re-sealed) *and* the stronger post-`Def-A1-1` variant where `stateHash` is also recomputed over the forged inner state, verifying both refuse with `WotrErrc::DamagedChecksum` (`ERR_DAMAGED_CHECKSUM`), leave the active `CampaignState` unmutated, and ensure a subsequent `saveCampaign` re-save does not keep the forged log (`ffff…f` absent, byte-equal to the legitimate save); (2) single-entry payload tampering (`acceptedCommands[1].payload.move.targetRegionId` rewritten while keeping its prior `checksum` and recomputing both `stateHash` and outer `checksum`).
- `docs/evidence/WOTR-1-1/Def-A1-2/report.md` — This acceptance report.
- `docs/evidence/WOTR-1-1/Def-A1-2/artifacts/standalone_doctest.txt` — Focused standalone `WotrRemaster` doctest run transcript.
- `docs/evidence/WOTR-1-1/Def-A1-2/artifacts/save_integrity_probe.txt` — Unmodified `docs/evidence/WOTR-1-1/A1/artifacts/a1_probe.cpp` execution output after the fix.
- `docs/evidence/WOTR-1-1/Def-A1-2/artifacts/a1_audit.txt` — Unmodified `docs/evidence/WOTR-1-1/A1/artifacts/a1_audit.cpp` 73-check execution output after the fix.

No files outside `Scope` (`Bridge/**`, `engine/CMakeLists.txt`, `campaigns/fixtures/**`, `docs/missions/**`, `godot/**`) were modified.

---

## 2. Design & Implementation Summary

### Defect Closed (`Def-A1-2`)
Auditor card `A1` (`docs/evidence/WOTR-1-1/A1/report.md` §7, `Def-A1-2`) showed that `loadCampaign()` parsed `acceptedCommands[*]` fields structurally without recomputing `envelopeChecksum(cmd)`. Even after `Def-A1-1` added `WotrSaveEnvelopeV1::stateHash`, an attacker who replaced `acceptedCommands[*].checksum` (for example with `ffff…f`) and recomputed both `e.stateHash = sha256(e.stateBytes)` and `e.checksum = sha256(saveEnvelopePayload(e))` would still pass `loadCampaign()`, making a forged command history durable across subsequent `saveCampaign()` calls.

### Solution
1. **Per-command cryptographic verification during transactional load (`Save.cpp`)**:
   - Inside `loadCampaign`, while parsing `stateRoot["acceptedCommands"]` into the temporary `CampaignState s`, each `CommandEnvelope cmd` is checked for `cmd.schemaVersion == 1` and `cmd.payload.type == CommandType::MoveArmy` (`WotrErrc::InvalidSave` on unsupported values so out-of-range enum casts cannot occur), and then cross-checked against `expectedCmdChecksum = envelopeChecksum(cmd)`.
   - If `cmd.checksum != expectedCmdChecksum`, `loadCampaign` immediately aborts with `fail<CampaignState>(WotrErrc::DamagedChecksum, ...)` before returning any `CampaignState` and before any caller state swap can occur.
2. **Envelope stability (Invariant 3)**:
   - `WotrSaveEnvelopeV1`, `saveEnvelopePayload`, `serializeSaveEnvelope`, `saveCampaign`, `kSaveEnvelopeVersion` (`1`), `contractVersion` (`1`), and `CampaignState::schemaVersion` (`1`) are completely unchanged. Legitimate saves produce the exact same bytes and SHA-256 digests as on `Base SHA`.

### Допущение (Autonomous Design Notes)
- **Допущение 1 (Named error code `WotrErrc::DamagedChecksum`)**: `docs/evidence/WOTR-1-1/A1/report.md` §7 (`Def-A1-2`, Expected column) explicitly names `ERR_DAMAGED_CHECKSUM` (`WotrErrc::DamagedChecksum = 11`), the same error code `resolveCommands` returns when `envelopeChecksum(e) != e.checksum` (`State.cpp:443`). Placing the `envelopeChecksum(cmd)` check inside the `acceptedCommands` parsing loop (prior to the outer `e.stateHash != parsedStateHash` check) ensures both unmodified `a1_probe` PROBE 3 and a fully re-hashed (`stateHash` + `checksum` recomputed) forged-command save deterministically fail with `ERR_DAMAGED_CHECKSUM`.

---

## 3. Verification & Evidence

### 3.1 Focused Standalone Build & `WotrRemaster` Doctest Suite
Command:
```bash
g++ -std=c++17 -O2 -Wall -Wextra -I engine/src -I engine/thirdparty/doctest \
  -DWOTR_FIXTURE_DIR=campaigns/fixtures/wotr_1_1 \
  engine/src/GameLogic/WotrRemaster/*.cpp engine/tests/WotrRemaster/*.cpp \
  -o /tmp/wotr-def-a1-2/wotr_tests && \
/tmp/wotr-def-a1-2/wotr_tests
```
- **Exit code**: `rc=0`
- **Binary SHA-256** (`/tmp/wotr-def-a1-2/wotr_tests`): `dafa43e139f3962659679a60f93c6f55a5ebcbdd724ebe95414b5275e9ed55eb`
- **Result**: `38 test cases: 38 passed | 0 failed | 0 skipped`, `369 assertions: 369 passed | 0 failed`
- **Transcript**: `docs/evidence/WOTR-1-1/Def-A1-2/artifacts/standalone_doctest.txt` (`sha256=3ef213e09c6fbe0fb4cc87bd53b59e1464d1019d8a4d76c298db079006c65bf5`)

### 3.2 Unmodified Auditor Save-Integrity Reproducer (`a1_probe.cpp`)
Command:
```bash
g++ -std=c++17 -O2 -Wall -Wextra -I engine/src \
  docs/evidence/WOTR-1-1/A1/artifacts/a1_probe.cpp engine/src/GameLogic/WotrRemaster/*.cpp \
  -o /tmp/wotr-def-a1-2/a1_probe && \
/tmp/wotr-def-a1-2/a1_probe campaigns/fixtures/wotr_1_1
```
- **Exit code**: `rc=0`
- **Output** (`docs/evidence/WOTR-1-1/Def-A1-2/artifacts/save_integrity_probe.txt`, `sha256=1174564952393eacdcaeb7e746f6106ace8acb32af7d43863ccd17eb5b8acd88`):
```text
BASELINE state_bytes_len=1812 state_hash=033cae70104415a3194ead818c87aad502ad3cf96c81f15021e5966df18e2887
PROBE1 dropped_armies_and_regions accepted=false err=ERR_INVALID_SAVE loaded_state_len=- loaded_state_hash=- equals_original=-
PROBE2 teleported_army accepted=false err=ERR_DAMAGED_SAVE_CHECKSUM loaded_army_region=-
PROBE3 forged_command_checksums accepted=false err=ERR_DAMAGED_CHECKSUM
PROBE4 forged_pending_request_checksum accepted=false err=ERR_DAMAGED_SAVE_CHECKSUM
PROBE5 legit_load ok=true bytes_equal=true
```

### 3.3 Unmodified Auditor Determinism & Contract Harness (`a1_audit.cpp`)
Command:
```bash
g++ -std=c++17 -O2 -Wall -Wextra -I engine/src \
  docs/evidence/WOTR-1-1/A1/artifacts/a1_audit.cpp engine/src/GameLogic/WotrRemaster/*.cpp \
  -o /tmp/wotr-def-a1-2/a1_audit && \
/tmp/wotr-def-a1-2/a1_audit campaigns/fixtures/wotr_1_1
```
- **Exit code**: `rc=0`
- **Summary**: `AUDIT_SUMMARY checks=73 failures=0`
- **Transcript**: `docs/evidence/WOTR-1-1/Def-A1-2/artifacts/a1_audit.txt` (`sha256=d95eea4a575de45904ef75319b470d92b682ea41425f7ec5a9acd00082c10239` — byte-identical to `Def-A1-1` baseline)
- **K2 Determinism & Envelope Stability Verification**:
  - `initial_state` (`1049` B): `29689fc52edb46a45a3960c641c9fe18803a4e9c59c54c8167d3b4b61ac28c73`
  - `after_resolve` (`1829` B): `ad8b96a15277817e9a00b1c44d93c6eb7963efd281336d972a1665e6632ea682`
  - `after_apply` (`1455` B): `1c0c0065cf8602ab18aac5dbc801904e43b2051fda7e06ae76817381d5f127f0`
  - `after_load_fresh_instance` (`1455` B): `1c0c0065cf8602ab18aac5dbc801904e43b2051fda7e06ae76817381d5f127f0`
  - `next_step_from_loaded` (`1749` B): `b78fb7c162c2b9dbf5edfd4aff90ac5145b9b84171ce3f254f944d2a9da9442a`
  - Save envelope: `envelope_len=2045`, `sha256=4eaac650fa890ddea36a34ae1eca9abc199cc8c0963724051ec99533e4d4d0dd` (unchanged).

### 3.4 Not Run (Declared Cost Gate `Build: focused`, `Retail: none`)

| Step | Status | Reason |
|---|---|---|
| `T2` full engine & GDExtension build (`tools/arena/build_engine.sh --tests`, `openbfme_tests`) | **not run** | Card `WOTR-1.1-Def-A1-2` declares `Build: focused` (`docs/missions/WOTR-1.1.md` §7, `WOTR-D-044`); envelope shape, CMake wiring, and GDExtension ABI are unchanged |
| `T3` retail archive hydration & smoke (`retail: hydrate`) | **not run** | Card declares `Retail: none`; pure strategic-core load validation |

---

## 4. Acceptance Criteria Check

| Requirement / Oracle / Invariant | Status | Evidence |
|---|---|---|
| Focused standalone build and doctest suite (`g++ -std=c++17 -O2 -Wall -Wextra`, `-DWOTR_FIXTURE_DIR=campaigns/fixtures/wotr_1_1`) exit 0 | PASS | `rc=0`, `38/38` test cases (`369` assertions) passed (`artifacts/standalone_doctest.txt`) |
| New regression test refuses A1 PROBE 3 reproducer (`acceptedCommands[*].checksum` replaced by `ffff…f`) with named error, and subsequent re-save does not keep forged log | PASS | `test_save.cpp` (`save: A1 PROBE 3 reproducer ...`) refuses both PROBE 3 and re-hashed `stateHash`+`checksum` variant with `ERR_DAMAGED_CHECKSUM`; subsequent `saveCampaign` matches legitimate envelope byte-for-byte with `ffff…f` absent; unmodified `a1_probe.cpp` outputs `PROBE3 forged_command_checksums accepted=false err=ERR_DAMAGED_CHECKSUM` |
| Invariant 1: Legitimate saves load byte-identically; §8 K2 determinism invariants hold unchanged | PASS | `a1_audit.cpp` `73/73` passed (`sha256=d95eea4a...` unchanged); `PROBE5 legit_load ok=true bytes_equal=true` |
| Invariant 2: Refusal happens before any state mutation and names an error | PASS | `loadCampaign` returns `fail<CampaignState>(WotrErrc::DamagedChecksum, ...)` before producing or swapping state (`res.value.acceptedCommands.empty()`, caller `session` bytes/hash unchanged) |
| Invariant 3: No envelope shape change and no version bump in this card | PASS | `WotrSaveEnvelopeV1` fields and `envelopeVersion=1`/`contractVersion=1`/`schemaVersion=1` untouched; `SAVE envelope_len=2045 sha256=4eaac650...` unchanged |
| Invariant 4: Writes only inside role zone / `Scope`; report at `docs/evidence/WOTR-1-1/Def-A1-2/report.md`; oracle not weakened | PASS | Only `Save.h`, `Save.cpp`, `test_save.cpp`, and `docs/evidence/WOTR-1-1/Def-A1-2/**` modified |

---

## 5. Known Limitations / Risks / Follow-ups

- **Relation to sibling card `Def-A1-3`**: `Def-A1-2` verifies every `acceptedCommands[*].checksum` via `envelopeChecksum()` at load. Binding `pendingBattle` presence to `phase == Phase::BattlePending`, `e.commandCursor == s.acceptedCommands.size()`, and pending battle request integrity (`a1_probe` PROBE 4 when re-hashed over `stateHash`) belongs to `WOTR-1.1 · Def-A1-3`.

---

## 6. Artifacts Index

| Path | Size (bytes) | SHA-256 |
|---|---:|---|
| `engine/src/GameLogic/WotrRemaster/Save.h` | `2393` | `b99b549ba3a2aa062d933412b4abb75cc0b12e40c5879b0c745fb5e7daa3edd6` |
| `engine/src/GameLogic/WotrRemaster/Save.cpp` | `28025` | `6322e154312e48f340f5a0df1162950024474e8bcefe2cb697ad3764ff32d68b` |
| `engine/tests/WotrRemaster/test_save.cpp` | `18229` | `1e680ad1c7dca84798b70e2b6929a96c3ac6afb1264030e0235313faa52d6efe` |
| `docs/evidence/WOTR-1-1/Def-A1-2/artifacts/standalone_doctest.txt` | `299` | `3ef213e09c6fbe0fb4cc87bd53b59e1464d1019d8a4d76c298db079006c65bf5` |
| `docs/evidence/WOTR-1-1/Def-A1-2/artifacts/save_integrity_probe.txt` | `521` | `1174564952393eacdcaeb7e746f6106ace8acb32af7d43863ccd17eb5b8acd88` |
| `docs/evidence/WOTR-1-1/Def-A1-2/artifacts/a1_audit.txt` | `4986` | `d95eea4a575de45904ef75319b470d92b682ea41425f7ec5a9acd00082c10239` |
