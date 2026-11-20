# War Of The Ring: Remaster — agent entry point

This repository is the single source tree for WOTR Remaster, its OpenBFME integration, process documents, and ScriptCat transport.

Before changing anything, read:

1. `docs/REGULATION.md` — authority, gates, and evidence rules;
2. `docs/PHASES.md` — phases 1–8 and the active mission;
3. the active specification in `docs/missions/`;
4. the matching role document in `docs/agents/roles/`;
5. `docs/agents/cost-gates.md` before building the engine or hydrating retail data.

## Non-negotiable architecture

- `WotrRemaster` is a separate deterministic strategic core. Do not make Godot nodes, wall-clock time, locale, rendering order, or unordered iteration part of simulation state.
- Godot owns the 2.5D strategic map, interaction, HUD, and Campaign Pack editor.
- OpenBFME Godot Nightly owns tactical battles and reads the player's retail BFME2/RotWK installation directly.
- The strategic/tactical boundary is a versioned native `BattleRequest` / `BattleResult` round trip. A mock can prove a contract, but cannot satisfy the retail integration gate.
- Retail files are never transformed or copied into product commits or reachable published history. Campaign Packs may reference retail content and may contain licensed custom content.
- Import legacy world data from `Creative36711/wotr` only through the accepted versioned one-way importer with stable IDs/provenance; do not adopt its React/Tauri/Win32 runtime or publish its dataset/BIG before license authorization.
- Count tactical capacity by opposed sides: allies share a start; side overflow uses a declared compatible fallback map or deterministic autoresolve. New maps are licensed content-addressed Campaign Pack overlays.
- Product locales are exactly `en`, `ru`, `zh-Hans`, `ja`, `pt-BR`, `de`, `fr`, `ko`; locale must never alter canonical state or hashes.

## Repository safety

- Work only on the branch supplied by Arena. Never reset, clean, or overwrite unrelated local changes.
- `BFME2/`, `RotWK/`, `.retail/`, and `.retail-transfer/` are retail-data areas. Treat them as read-only unless the active card explicitly concerns byte-exact transfer or validation.
- `tools/script-cat/**` is protected transport code. Changes require a Transport Maintainer card, a version bump, a rebuilt bundle, portability guard, and the complete transport test gate.
- Tests and acceptance oracles may be extended, never weakened, skipped, or rewritten to fit an implementation.
- One card means one coherent change and one evidence trail. Stay inside both the role zone and the card Scope.

## Cost gates

Every card declares `build: none|focused|full` and `retail: none|reuse|hydrate`. Start with static/focused checks. Do not run a full engine build or download the 5.249-GiB retail snapshot merely as a precaution; do run them when the affected boundary or final oracle requires them. Never increase the declared tier silently. The normative triggers, Drive canary/resume procedure and command matrix are in `docs/agents/cost-gates.md`.

## Evidence

A claim is accepted only with the declared/actual Cost gate, exact command, return code, relevant output, artifact path, and tested SHA. `not run`, timeout, a green unrelated CI job, or a mock in place of the required retail path is not a pass. Visual work requires opening the produced image or recording and describing what is actually visible.

Process documents are written in Russian. Code, identifiers, schemas, and commit subjects are written in English unless an upstream convention requires otherwise.

## OpenBFME edition fidelity

The tactical downstream keeps the upstream edition rules below. They apply in addition to the WOTR mission and role gates.

OpenBFME is growing two editions of the game, each with its own rules:

- **BFME2 1.06**: the base. Its rules come from the clean BFME2 1.06
  `game.dat` (md5 `34391b253b0ee12d6cec1178f46d3bf9`); the Open-BFME-2
  decompilation is the reading aid, not the authority.
- **RotWK 2.01**: BFME2 plus its differences. Its rules come from the RotWK
  2.01 `game.dat` (cited as `RW 0x...`).

Today every game runs RotWK's rules. The BFME2 edition exists only behind a
developer switch (`OPENBFME_EDITION=bfme2`, see `docs/EDITIONS.md`). BFME1
keeps its current mode (RotWK's rules on BFME1 data, stop S-5017).

## Rules for every change

1. **State the editions.** Every simulation change says in its commit /
   pull request / report which editions it affects ("RotWK only", "both",
   "BFME2 only").
2. **Cite both sources where the games differ.** A function whose behaviour
   differs between the editions cites the RotWK address (`RW 0x...`) and the
   BFME2 one (the BFME2 1.06 `game.dat` address, `BFME2 0x...`, or the
   decomp's file / `Rva` row). `tools/re/rw2decomp.py` and
   `tools/re/pair_diff.py` find the pair; `tools/re/edition_census.py` lists
   which functions differ.
3. **Never fall back silently.** A rule that one edition has and the other
   lacks is a named error, an `unverified` report line or a registered stop
   (`docs/STOPS.md`), never the other edition's rule used quietly.
4. **Per-system variants, not scattered checks.** Edition differences live
   in one variant per system (AI, hordes, castles, War of the Ring, ...)
   reached through one edition accessor, not in per-function `if` tests on
   strings or install paths spread through the code.
5. **Mods declare their edition.** A mod is loaded for the edition it names.
6. **Check every edition.** A change to a shared parser, data table or startup
   path is checked against BFME1, BFME2 and RotWK data; a field another
   edition uses is handled or reported, never rejected by accident.

Everything else (sources, numerics, tests, the commit gate, privacy) is in
`CLAUDE.md`, `docs/PLAN.md` and `REVIEW.md`.
