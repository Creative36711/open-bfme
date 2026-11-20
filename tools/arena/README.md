# Arena helpers

These scripts reproduce the OpenBFME build and art environment in an ephemeral
Arena sandbox.

They were ported from `Creative36711/advanced-randomizer` at commit
`cda1b7d9b12beefc75b8f7cd440b8dd239fec8ad`, then adapted for a downstream
OpenBFME tree at the repository root. Notable changes include:

- runtime/art bootstrap profiles;
- actual SwiftShader extraction and a persistent launch wrapper;
- SHA-256 verification of all three vendor archives and the final Godot binary;
- dynamic `godot-cpp` pin resolution from the current upstream gitlink;
- a runnable no-tests build as the default;
- removal of paths and claims specific to Advanced Randomizer/Chaos.

Use `bootstrap.sh --check` to inspect the environment without changing it.

Build cost is selected by the card, not by habit (`docs/agents/cost-gates.md`):

- `build_engine.sh --check` for inventory;
- `--quick` or `--core` for a focused edit/compile loop;
- `--tests` for the full source handoff gate;
- the default runnable extension only when a Godot/runtime check needs it.

Retail hydration is separate and is never implied by these scripts. It requires a declared `retail: hydrate`, metadata/canary checks and the resumable `tools/retail_big_transfer.py ingest` flow.
