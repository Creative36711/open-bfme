# Pinned Godot runtime for Arena

Godot `4.7.2.stable.custom_build.ed1daf0bf` and SwiftShader are needed because
Arena has no physical GPU. Their compressed archives are deliberately **not**
committed to this public repository.

`tools/arena/bootstrap.sh --runtime` hydrates the three archives into
`$WORK/openbfme-godot-vendor` only when the runtime is explicitly requested. It
reads them from public repository `Creative36711/advanced-randomizer` at pinned
commit `cda1b7d9b12beefc75b8f7cd440b8dd239fec8ad`, verifies every archive against
`SHA256SUMS`, reconstructs Godot under `$WORK/godotbin`, and verifies the
uncompressed executable before use. `--art` uses the same pinned runtime.

The hashes and source commit preserve reproducibility without placing the
63-MiB tool archives in OpenBFME's public tree. Delete the two `$WORK` cache
folders to force a clean hydration.
