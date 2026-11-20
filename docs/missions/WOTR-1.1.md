# WOTR-1.1 · Контракты и детерминированный walking skeleton

ТЗ 1.0 · статус: Активна · фаза 1 · владелец: Warden · 2026-11-20.

Notion draft: [WOTR-1.1](https://app.notion.com/p/WOTR-1-1-41e6f0cf570581d89a15eca7e646f077).

## 1. Намерение

Доказать самый короткий реальный вертикальный путь нового продукта:

```text
Campaign Pack → strategic map → WEGO movement → conflict
→ native BattleRequest → OpenBFME tactical battle
→ native full BattleResult → strategic map → canonical save/load/hash
```

Результат миссии — не набор интерфейсов и не демонстрационный mock, а воспроизводимый round-trip в Arena на retail BFME2 1.06 + RotWK 2.01.

## 2. Baseline и зависимости

- Один репозиторий: `Creative36711/open-bfme`.
- OpenBFME pin: `v0.4.12-nightly.261118100`, commit `4a8d82727c2664c381b7b1c07344881760c4765b`.
- Godot line: 4.7; engine: C++17 GDExtension.
- Tactical profile: Vanilla RotWK 2.01.
- Existing upstream tactical facts may be wrapped: `LivingWorld::BattleSetup`, `LivingWorld::BattleResult`, `BattleResultCapture`, `NewGameMessage::livingWorldBattle`, retail archive policy.
- Existing `LivingWorldLogic` is reference/read-only for this mission; it is not the Remaster strategic core.
- Required roles: Upstream Integrator, Strategic Core Engineer, Tactical Bridge Engineer, Godot World Engineer, Determinism & Save Auditor, Integration Auditor. UX & Modding Auditor reviews the visible map and pack boundary.

Execution begins only after the required role rows have active Arena Agent URLs and the published source baseline is present in the repository.

## 3. Инварианты

- **INV-1 Separate core.** Authoritative state and rules live under `GameLogic/WotrRemaster`, not in GDScript and not in retail-faithful `LivingWorldLogic`.
- **INV-2 Deterministic input.** Pack hash + initial seed + ordered command envelopes produce identical state bytes and hashes.
- **INV-3 No ambient input.** Simulation does not read wall clock, locale, Godot node state, absolute paths, filesystem order or unordered container order.
- **INV-4 Native typed boundary.** Tactical launch/return uses versioned C++ DTOs. Godot Dictionaries may expose a binding but are not canonical storage.
- **INV-5 Full result.** Result is not only winner/casualties: persistent fields listed in §6 are carried or explicitly represented as empty/not-applicable.
- **INV-6 Transactional apply.** Wrong battle ID, request checksum, state hash, unknown entity, duplicate result or unsupported contract version changes zero strategic bytes.
- **INV-7 Retail truth.** Final tactical criterion uses OpenBFME with actual validated retail archives. A fake result cannot satisfy it.
- **INV-8 Retail read-only.** No retail asset or transformed derivative is committed by this mission.
- **INV-9 Save identity.** Save includes pack identity/hash and tactical profile; corrupt/unknown/incompatible input fails before state swap.
- **INV-10 Existing OpenBFME parity.** Focused upstream tests and ordinary skirmish startup remain green.
- **INV-11 One conflict.** The fixture contains one intentional movement conflict and one tactical battle; no economy, diplomacy or narrative system is built.
- **INV-12 Legacy boundary.** No code or mechanics are imported from `Creative36711/wotr`.

## 4. Required product slice

### 4.1 Tiny Campaign Pack

A versioned synthetic pack, committed without retail content:

- three connected hex/regions: `west`, `crossing`, `east`;
- two factions and one controllable army each;
- stable IDs for every region, faction, army and member;
- one tactical map reference known to Vanilla RotWK 2.01;
- simple visual placement and labels;
- one move from each side into `crossing`, creating a conflict;
- manifest, schema version, content hash and validation diagnostics.

### 4.2 Core flow

1. Load and validate pack.
2. Build initial state from explicit seed.
3. Accept two move command envelopes.
4. Resolve canonical command order.
5. Move armies and create exactly one pending battle.
6. Emit immutable `BattleRequest` and freeze that pending battle until a result arrives.
7. Apply valid `BattleResult` once.
8. Return to planning state with updated armies/ownership.
9. Save, load into a fresh instance and compare canonical bytes/hash.

### 4.3 Godot flow

A minimal 2.5D view must allow the user/test driver to:

- see the three cells and two armies;
- select the local army and preview/submit its move;
- see validation and conflict state;
- transition into the OpenBFME battle without launching another application;
- return to the same strategic view with visible survivor/owner changes;
- save and reload the resulting campaign.

The flow needs headless automation and one opened visual artifact. Visual polish is not part of this mission.

## 5. Canonical strategic contracts

Names may change only by accepted ADR; semantics may not.

```text
CampaignPackManifest
CampaignState
CommandEnvelope<MoveArmy>
ResolutionEvent
PendingBattle
BattleRequestV1
BattleResultV1
WotrSaveEnvelopeV1
```

Stable IDs are strings or fixed-width values with explicit normalization. No Godot instance ID or container address crosses the boundary. Every decoder returns structured error code + context; user-facing localization remains outside core.

## 6. Tactical round-trip

### BattleRequestV1 must carry

- contract version, battle ID and strategic state hash;
- pack ID/hash, tactical profile and map reference;
- tactical seed, participants, teams and human/AI seats;
- army/member stable IDs, templates, quantities, experience/rank and persistent upgrades;
- hero identity and persistence metadata;
- starting resources, persistent sciences/powers and reinforcement schedule;
- strategic building/objective IDs represented on the tactical map;
- canonical request checksum.

### BattleResultV1 must carry

- contract version, battle ID, request checksum and completion reason;
- winner/draw/forfeit identity;
- survivors by stable army/member identity, quantity, experience/rank and persistent upgrades;
- hero survival/death/revival records;
- per-player persistent sciences/upgrades/skill or power state;
- called and uncalled reinforcements;
- destroyed strategic building/objective IDs;
- tactical statistics required for strategic presentation;
- deterministic capture boundary/frame where available;
- canonical result checksum.

If OpenBFME cannot currently produce a field, the DTO still represents it with explicit absence and the card reports the exact gap. Silently dropping a persistent field is failure.

## 7. Cards

| Card | Role | Depends on | Required output |
|---|---|---|---|
| `WOTR-1.1-K1` | Upstream Integrator | — | pinned OpenBFME source baseline in the one repository; sync report; baseline build/tests |
| `WOTR-1.1-K2` | Strategic Core Engineer | K1 | pack loader, state/commands, deterministic conflict, DTOs, canonical save/hash and pure tests |
| `WOTR-1.1-K3` | Tactical Bridge Engineer | K1,K2 | adapter to native OpenBFME start/result capture and focused bridge tests |
| `WOTR-1.1-K4` | Godot World Engineer | K2,K3 | minimal 2.5D interaction and automated route through battle/return/save |
| `WOTR-1.1-A1` | Determinism & Save Auditor | K2,K4 | independent dual-run and corruption/transactionality report |
| `WOTR-1.1-A2` | UX & Modding Auditor | K4 | opened visual artifact and pack/reference boundary report |
| `WOTR-1.1-A3` | Integration Auditor | K1–K4,A1,A2 | clean-checkout Arena retail end-to-end evidence |

Warden may split K2–K4 further only if the final oracle and interfaces remain unchanged. Coordinator must not issue overlapping write zones in parallel.

## 8. Card-level oracles

### K1 · Source baseline

- Exact upstream pin equals `OPENBFME_UPSTREAM`.
- GPL `LICENSE`/`NOTICE` are preserved.
- Clean configure and focused build of `openbfme` and `openbfme_tests` return 0.
- Baseline WOTR/tactical tests selected from upstream run and their names/results are recorded.
- Source import does not alter `BFME2/`, `RotWK/`, user dirty files or ScriptCat.

### K2 · Core

- Schema accepts the valid tiny pack and rejects duplicate IDs, broken topology, missing tactical profile/map and unstable/absolute references.
- Two fresh processes run the same commands and emit byte-identical canonical state plus equal hash at every resolution boundary.
- Reordered source JSON properties do not change canonical pack/state hash.
- Invalid command and invalid result leave pre/post state bytes equal.
- Save/load in a fresh instance yields equal bytes/hash and continues with equal next-step hash.
- Unknown save/contract version and damaged checksum return named nonzero failures.

### K3 · Bridge

- Synthetic DTO → native start → synthetic native capture → DTO round-trip proves every field mapping.
- Negative cases reject unknown templates/entities, duplicate battle completion and mismatched checksum/hash transactionally.
- A real OpenBFME tactical map starts from the request and native `BattleResultCapture` (or an accepted equivalent end boundary) produces the result; no hand-authored result on this path.
- Existing focused OpenBFME tactical/WOTR tests remain green.

### K4 · Godot

- Headless scenario performs select → move → conflict → battle launch → return → save/load and exits 0.
- Input is translated into core commands; no direct state mutation from GDScript.
- Screenshot or short recording shows the tiny map before and after battle. Agent opens it and reports visible cells, armies, route/conflict and result change.
- Frame time and peak memory are reported as facts, without setting a release budget in this mission.

### A1 · Determinism/save

Auditor reruns from a clean checkout, on accepted SHA, with separate output directories. At least two complete runs have equal command log, state bytes and hash. Corrupt, unknown-version and mismatched-pack inputs fail before state mutation. Auditor does not reuse executor-generated pass files as proof.

### A2 · UX/modding

Auditor opens the artifact at normal scale, checks selection/readability and confirms the tiny pack has no absolute path, copied retail data or undeclared custom asset. Vanilla profile resolution is explicit.

### A3 · Integration

Auditor performs §9 exactly with actual retail paths in Arena and records exact commands, rc, relevant logs, hashes and artifact paths. A skipped tactical launch, test hook that kills units, injected result or mocked asset provider is failure for the final criterion.

## 9. Final oracle of the mission

On one accepted SHA, from a clean checkout in Arena:

1. Verify exact OpenBFME pin and clean source tree.
2. Configure/build the C++ GDExtension and focused tests.
3. Set `BFME2_INSTALL` and `ROTWK_INSTALL` to validated retail trees; retail archive validation passes.
4. Launch the automated WOTR Remaster fixture through Godot/OpenBFME.
5. Observe and artifact the initial three-cell map.
6. Submit movement through the ordinary command API and reach one conflict.
7. Start a real tactical map through OpenBFME with request checksum logged.
8. Complete the tactical battle through an accepted deterministic test scenario that still runs native tactical logic; capture a native full result. A hand-authored result is forbidden.
9. Apply result once; second apply is rejected and leaves the hash unchanged.
10. Return to the map; artifact shows the changed surviving force/ownership.
11. Save, load in a fresh process and prove byte-identical canonical state/hash.
12. Run the selected upstream regressions and all WOTR-1.1 tests with `fail 0`.

Acceptance record includes:

- BASE_SHA, HEAD_SHA and merge SHA;
- upstream tag/commit;
- exact commands and return codes;
- request/result/state/save hashes;
- C++/Godot test summaries;
- initial/final visual artifact paths and auditor descriptions;
- list of any `not run` items. Any required item marked `not run` means `rework`.

## 10. Scope

### Allowed by cards

- `OPENBFME_UPSTREAM` and sync tooling/report for K1;
- imported upstream source and required license/notice files for K1;
- `engine/src/GameLogic/WotrRemaster/**` and focused tests for K2;
- narrow OpenBFME/Godot adapter points named by K3;
- `godot/wotr_remaster/**` for K4;
- `campaigns/schema/**`, `campaigns/fixtures/wotr_1_1/**` for the tiny pack;
- additive test/evidence documents explicitly named by audit cards.

### Read-only or forbidden

- `BFME2/**`, `RotWK/**` and retail binaries;
- `tools/script-cat/**` and relay files;
- unrelated OpenBFME parity code;
- existing tests/oracles except additive wiring required by the card;
- mechanics or files from `Creative36711/wotr`;
- systems assigned to later phases.

## 11. Not in this mission

Full Middle-earth map, production art, economy/recruitment depth, narrative events, diplomacy, deep AI, multiplayer, mod discovery UI, retail Living World parity, balance and migration from another WOTR implementation.

## 12. Allowed freedom

Choice of canonical binary format, hash algorithm, exact C++ namespace decomposition, tiny-map geometry and Godot rendering primitive is free if all invariants/oracles pass and the choice is documented. Dependency additions, public schema changes, changes to OpenBFME faithful behavior or a reduced battle result require ADR.

## 13. Rollback

The feature remains behind an explicit WOTR Remaster route until acceptance. Rollback removes the route, `WotrRemaster` modules and fixture without modifying retail data or ordinary OpenBFME saves. WOTR saves carry a distinct magic/version and are never opened as ordinary OpenBFME saves.
