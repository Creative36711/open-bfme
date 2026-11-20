# War Of The Ring: Remaster — agent entry point

This repository is the single source tree for WOTR Remaster, its OpenBFME integration, process documents, and ScriptCat transport.

Before changing anything, read:

1. `docs/REGULATION.md` — authority, gates, and evidence rules;
2. `docs/PHASES.md` — phases 1–8 and the active mission;
3. the active specification in `docs/missions/`;
4. the matching role document in `docs/agents/roles/`.

## Non-negotiable architecture

- `WotrRemaster` is a separate deterministic strategic core. Do not make Godot nodes, wall-clock time, locale, rendering order, or unordered iteration part of simulation state.
- Godot owns the 2.5D strategic map, interaction, HUD, and Campaign Pack editor.
- OpenBFME Godot Nightly owns tactical battles and reads the player's retail BFME2/RotWK installation directly.
- The strategic/tactical boundary is a versioned native `BattleRequest` / `BattleResult` round trip. A mock can prove a contract, but cannot satisfy the retail integration gate.
- Retail files are never transformed or copied into product commits. Campaign Packs may reference retail content and may contain licensed custom content.
- Do not import mechanics from `Creative36711/wotr` without a separate owner decision.

## Repository safety

- Work only on the branch supplied by Arena. Never reset, clean, or overwrite unrelated local changes.
- `BFME2/`, `RotWK/`, `.retail/`, and `.retail-transfer/` are retail-data areas. Treat them as read-only unless the active card explicitly concerns byte-exact transfer or validation.
- `tools/script-cat/**` is protected transport code. Changes require a Transport Maintainer card, a version bump, a rebuilt bundle, portability guard, and the complete transport test gate.
- Tests and acceptance oracles may be extended, never weakened, skipped, or rewritten to fit an implementation.
- One card means one coherent change and one evidence trail. Stay inside both the role zone and the card Scope.

## Evidence

A claim is accepted only with the exact command, return code, relevant output, artifact path, and tested SHA. `not run`, timeout, a green unrelated CI job, or a mock in place of the required retail path is not a pass. Visual work requires opening the produced image or recording and describing what is actually visible.

Process documents are written in Russian. Code, identifiers, schemas, and commit subjects are written in English unless an upstream convention requires otherwise.
