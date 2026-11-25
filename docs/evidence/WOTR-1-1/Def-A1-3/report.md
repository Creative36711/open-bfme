# Acceptance Report — WOTR-1.1 · Def-A1-3

## Card and repository

- **Dispatch / run:** `4226f0cf-5705-8165-9f7f-f2f11a7cea69` / `3085dbac-74e1-48ec-89ea-20e3f7bc5107`
- **Mission / role:** `WOTR-1.1` / Strategic Core Engineer (Executor)
- **Target:** `Creative36711/open-bfme`
- **Branch:** `arena/e60ab74a-open-bfme` (fixed; not renamed)
- **Starting commit:** `8ef2e794794f4d6f68ff594cf25dc0b15ac7c2ed`
- **Starting shallow check:** `git rev-parse --is-shallow-repository` -> `true`; fetched/unshallowed `origin/main` and verified `git merge-base HEAD origin/main` -> `f3eec31f8f8cbb62d6cf500d9348eaf47bd8b20e` before merging main into this branch. No unrelated working-tree changes were present.
- **Current base incorporated:** `origin/main` at `d98a85f66b8589ae71465d3f24c611710154b9fe` (prior Def-A1-2 squash merge).
- **Cost gate:** focused standalone build; retail not run (not requested).
- **Report SHA:** `docs/evidence/WOTR-1-1/Def-A1-3/report.md` (this report; commit after verification).

## Implementation

`loadCampaign` now validates a present `pendingBattle` before accepting the parsed state:

1. `phase` must be `Phase::BattlePending`; otherwise return named `ERR_INVALID_SAVE`.
2. The envelope `commandCursor` must equal the parsed `acceptedCommands.size()`; otherwise return named `ERR_INVALID_SAVE`.
3. Reconstruct the immutable `BattleRequestV1` from the loaded strategic state and pending battle participants, recalculate `battleRequestChecksum`, and reject a mismatch as `ERR_DAMAGED_SAVE_CHECKSUM`. This prevents a re-sealed save with a forged `pendingBattle.requestChecksum` from loading and later rejecting the legitimate battle result with `ERR_MISMATCHED_CHECKSUM`.

All checks operate on the temporary state assembled by `loadCampaign`; no state is returned on refusal. The `WotrSaveEnvelopeV1` shape and `PendingBattle` in-memory representation remain unchanged. A legitimate pending battle save loads and applies its legitimate result successfully.

### Допущение:
The card specifies named errors for refusal but does not prescribe exact error codes for the phase/cursor structural contradiction or the forged request digest. Used `ERR_INVALID_SAVE` for the contradictory phase/cursor invariants, and `ERR_DAMAGED_SAVE_CHECKSUM` for the reconstructed battle-request checksum mismatch. This follows existing save-validation conventions and keeps error handling deterministic without changing the contract shape.

## Changed files

All changes are within card Scope:

- `engine/src/GameLogic/WotrRemaster/Save.cpp`
- `engine/tests/WotrRemaster/test_save.cpp`
- `docs/evidence/WOTR-1-1/Def-A1-3/report.md`
- `docs/evidence/WOTR-1-1/Def-A1-3/artifacts/standalone_doctest.txt`
- `docs/evidence/WOTR-1-1/Def-A1-3/artifacts/save_integrity_probe.txt`
- `docs/evidence/WOTR-1-1/Def-A1-3/artifacts/a1_audit.txt`

No schema, fixture, CMake, Bridge, mission, or retail files were modified. Existing tests/oracles were not weakened.

## Regression coverage and verification

### Focused standalone build + doctest

Command:
```bash
g++ -std=c++17 -O2 -Wall -Wextra -I engine/src -I engine/thirdparty/doctest \
  -DWOTR_FIXTURE_DIR=campaigns/fixtures/wotr_1_1 \
  engine/src/GameLogic/WotrRemaster/*.cpp engine/tests/WotrRemaster/*.cpp \
  -o /tmp/wotr-def-a1-3/wotr_tests && /tmp/wotr-def-a1-3/wotr_tests
```

- **Return code:** `0`
- **Result:** `39/39` test cases; `392/392` assertions passed.
- **Binary SHA-256:** `ac380436597c0155074d2ea5daa5b6b226312bbcb97222191c3532f37fb287f2`
- **Transcript SHA-256:** `e3794ff030397ba4e2f395c10b52121f232e2e8670b6e1ce4ff96cf43a8af838`
- **Artifact:** `docs/evidence/WOTR-1-1/Def-A1-3/artifacts/standalone_doctest.txt`

New regression case exercises all requested save conditions after re-sealing `stateHash` and outer envelope checksum: request checksum set to 64 zeroes is refused with `ERR_DAMAGED_SAVE_CHECKSUM`; `phase=Planning` with pending battle and a cursor unequal to `acceptedCommands.size()` are each refused with `ERR_INVALID_SAVE`. It also verifies a valid pending-battle save round-trips and its real result resolves to the same state as the uninterrupted flow.

### Unmodified auditor reproducer and determinism audit

- `a1_probe.cpp` focused build/run, **return code 0**, binary SHA-256 `cf1c232203887062286f903327a7f3069883667d61337c79154b345bbeebab01`. PROBE 4 refuses the forged request checksum as `ERR_DAMAGED_SAVE_CHECKSUM`; PROBE 5 confirms `legit_load ok=true bytes_equal=true`. Transcript: `docs/evidence/WOTR-1-1/Def-A1-3/artifacts/save_integrity_probe.txt`, SHA-256 `1174564952393eacdcaeb7e746f6106ace8acb32af7d43863ccd17eb5b8acd88`.
- `a1_audit.cpp` focused build/run, **return code 0**, binary SHA-256 `db68d8a64f7c33e58e460bb7472f3eabcc7629aaad855c6d9ee5f48c64943c29`. `AUDIT_SUMMARY checks=73 failures=0`; all K2 boundary hashes and save-envelope bytes remain unchanged (envelope length `2045`, SHA-256 `4eaac650fa890ddea36a34ae1eca9abc199cc8c0963724051ec99533e4d4d0dd`). Audit output is byte-identical to the prior accepted baseline (`d95eea4a575de45904ef75319b470d92b682ea41425f7ec5a9acd00082c10239`). Artifact: `docs/evidence/WOTR-1-1/Def-A1-3/artifacts/a1_audit.txt`.

Focused commands produced only existing unused-function/parameter warnings; all completed with return code zero. Full engine build and retail hydration were not run (outside this card's focused-build need). `python3 tools/precommit.py` passed (`commit gate ok`, 6 changed files checked). `python3 tools/release/pr_audit.py --tree <staged-tree> --base origin/main` returned `1`; it reported 0 findings in this tree and 16 inherited findings from `origin/main` (primarily pre-existing workflow policy checks and unavailable PyYAML). No inherited files were changed by this card.
