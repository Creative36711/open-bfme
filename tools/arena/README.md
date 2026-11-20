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
