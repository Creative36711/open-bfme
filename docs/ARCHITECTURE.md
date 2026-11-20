# WOTR Remaster architecture

Статус: accepted baseline · 2026-11-20.

## 1. System context

```text
Campaign Pack ──> WotrRemaster deterministic core ──> canonical state/save/hash
      │                         │
      │                         ├──> Godot 2.5D map + editor (presentation/input)
      │                         │
      └─ asset/profile refs     └──> Tactical adapter
                                         │ BattleRequest
                                         v
                                OpenBFME Godot Nightly
                                + user's retail BFME2/RotWK
                                         │ BattleResult
                                         └──────────────> core apply
```

Стратегия и editor — продукт WOTR Remaster. OpenBFME — tactical runtime и retail asset provider. Интеграция остаётся нативной внутри одного build; JSON допустим для fixtures/debug export, но не является обязательным runtime IPC.

## 2. Upstream baseline

| Поле | Значение |
|---|---|
| Repository | `Open-BFME/openbfme-godot` |
| Profile | `v0.4.12-nightly.261118100` |
| Commit | `4a8d82727c2664c381b7b1c07344881760c4765b` |
| License | GPL-3.0 |
| Godot | 4.7 line |
| Tactical rules | C++17 GDExtension under `engine/` |
| Presentation/runtime | Godot project under `godot/` |

Pin is machine-readable in `OPENBFME_UPSTREAM`. Upstream already exposes validated WOTR tactical pieces such as `LivingWorld::BattleSetup`, `LivingWorld::BattleResult`, `BattleResultCapture`, Godot `LivingWorldGame`, retail archive policy and WOTR save/test fixtures. WOTR Remaster may adapt those tactical contracts, but does not adopt `LivingWorldLogic` as its strategic source of truth.

## 3. Target repository layout

```text
engine/
  src/GameLogic/WotrRemaster/       pure deterministic strategy
  src/GodotDevice/                  narrow Godot bindings and tactical adapter
  tests/WotrRemaster/               core, contract, save and bridge tests
godot/
  wotr_remaster/
    world/                           2.5D map and HUD
    editor/                          Campaign Pack editor
    tests/                           headless interaction/visual tests
campaigns/
  schema/                            versioned machine-readable schemas
  vanilla_rotwk_201/                 references/data, no copied retail assets
  fixtures/                          synthetic tiny packs
art/                                 licensed custom strategic assets only
docs/                                process, architecture, missions and ADR
 tools/script-cat/                   agent transport (protected)
```

Фактические пути утверждаются первой карточкой source integration; смысл границ менять нельзя.

## 4. Deterministic core

### 4.1 State

Минимальный `CampaignState`:

- schema and simulation version;
- Campaign Pack identity and content hash;
- turn, phase and deterministic sequence counters;
- factions/players and team relations;
- regions/hex/topology identifiers;
- armies, heroes, persistent unit records and ownership;
- resources and queued actions required by the active ruleset;
- named RNG streams and their state;
- pending battle identity and lifecycle;
- accepted command log cursor.

Presentation fields, localized strings, absolute paths, timestamps and Godot object IDs are not state.

### 4.2 Commands and WEGO

Every mutation enters through a typed command:

```text
CommandEnvelope {
  schema_version,
  campaign_id,
  turn,
  player_id,
  client_sequence,
  command_id,
  payload
}
```

Pipeline: decode → structural validation → authorization → rules validation → canonical ordering → apply → events → hash. A simple AI emits the same commands as a human. No AI-only mutation API exists.

Canonical ordering must be explicit and stable: phase, initiative rule from the Campaign Pack, player ID, client sequence, command ID. Any tie-break uses a named deterministic RNG stream and records the draw purpose.

### 4.3 Serialization and hash

- Fixed field order and numeric representation.
- Collections serialized by stable key/order, never container iteration accident.
- Unknown schema version is rejected before mutation.
- Hash covers every simulation-relevant field and excludes presentation/cache fields by contract.
- Save load is transactional: parse and validate into a new state, then swap on success.

## 5. Strategic/tactical contract

### 5.1 BattleRequest

The request is immutable after launch and contains at least:

- contract version and unique battle ID;
- campaign/state hash at dispatch;
- tactical profile and map reference;
- region/terrain context used by strategic resolution;
- participants, teams, human/AI assignment and deterministic tactical seed;
- armies and stable strategic army/member IDs;
- templates, quantities, experience/rank and persistent upgrades;
- heroes and revival/persistence metadata;
- starting resources, powers/sciences and allowed reinforcements;
- strategic buildings/objectives that tactical play may destroy;
- return policy and checksum.

The adapter maps this into OpenBFME's native start structures. The core never depends on a Godot Dictionary as its canonical representation.

### 5.2 BattleResult

The result contains at least:

- contract version, battle ID, request checksum and completion reason;
- victor/draw/forfeit identity;
- each surviving strategic member with stable army/member ID, quantity, experience/rank and persistent upgrades;
- hero survival/death/revival records;
- per-player persistent sciences/upgrades/skill or power state;
- reinforcements called and never called;
- strategic buildings/objectives destroyed;
- tactical statistics needed by campaign rules;
- deterministic capture frame or equivalent tactical completion boundary;
- result checksum and explicit validation errors.

Apply is idempotent by battle ID. A result for the wrong state hash, unknown army/member, duplicate completion or incompatible contract version is rejected without partial mutation.

### 5.3 Reuse boundary

OpenBFME's `BattleResultCapture` is valuable because it captures at a deterministic end-of-frame boundary and already reads native survivors, hero records and persistent player data. The WOTR adapter may wrap it. Direct calls from Godot world UI into strategic mutation are prohibited.

## 6. Godot world and editor

The map is 2.5D: strategic coordinates/topology are data; world meshes, labels and overlays are presentation. Hex rendering may aggregate into named regions, but selection and movement validation use core IDs.

Godot responsibilities:

- render terrain, hex/region overlays, armies and routes;
- translate user gestures into command drafts;
- display validation errors and resolution events;
- launch/return from tactical runtime through the adapter;
- edit and validate Campaign Packs;
- never own the authoritative campaign state.

The editor uses commands and supports undo/redo as inverse or snapshot-safe operations. Export → reopen must produce the same canonical content hash.

## 7. Campaign Pack

A pack is versioned data with:

- manifest, ID/version, compatible engine and tactical profiles;
- topology/hex/region definitions and visual placement;
- faction/player/army/hero definitions;
- ruleset parameters, starts and objectives;
- tactical map/template references;
- custom asset manifest with license/provenance/hash;
- dependencies and deterministic override order;
- migration metadata.

Reference kinds are explicit (`retail:`, `pack:`, `custom:` or equivalent accepted schema). Absolute author-machine paths are invalid. Vanilla and mod packs pass the same validator.

## 8. Retail boundary

Retail BFME2 1.06 and RotWK 2.01 are user-supplied and read-only. OpenBFME's archive policy validates expected files by size/hash and mounts original archives in a defined order. Product tests receive locations through `BFME2_INSTALL` and `ROTWK_INSTALL` or the runtime's normal install-path configuration.

No retail binary is added by WOTR product commits. Byte-exact transfer tooling is operational support, not a content packaging mechanism. A canary part is verified before any bulk transfer.

## 9. Save envelope

```text
WotrSave {
  magic,
  envelope_version,
  engine_version,
  campaign_pack_id,
  campaign_pack_hash,
  tactical_profile,
  state_bytes,
  state_hash,
  command_cursor,
  rng_metadata,
  checksum
}
```

Mid-battle saves additionally carry immutable `BattleRequest` identity and the tactical save reference required by OpenBFME. On return, result identity must match both.

## 10. Testing pyramid

1. Pure unit/property tests with tiny synthetic packs.
2. Deterministic replay and save/load dual runs.
3. Native adapter contract tests against OpenBFME types.
4. Godot headless map/editor smoke.
5. Retail tactical integration on a known map.
6. Arena end-to-end walking skeleton with screenshot/log/save/hash artifacts.
7. Upstream sync gate replaying levels 1–6.

The final level cannot be replaced by mocks or serialized hand-written results.

## 11. Upstream synchronization

Upstream Integrator advances only the explicit pin. The report includes old/new commits, upstream commit range, file-level conflicts, local WOTR-owned paths, license/notice changes and exact gates. A sync that compiles but loses the tactical result, save hash, retail validation or Godot route is rejected.

WOTR extensions should be additive and concentrated in `WotrRemaster` paths and narrow adapters, minimizing edits to faithful OpenBFME systems. Generally useful bridge fixes should be prepared so they can be contributed upstream without exposing WOTR product policy.
