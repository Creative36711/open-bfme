# Pinned Godot runtime for Arena

Godot `4.7.2.stable.custom_build.ed1daf0bf` and SwiftShader are stored here
because Arena cannot reach GitHub release-asset hosts and has no physical GPU.
The Godot xz stream is split below GitHub's 50-MiB warning threshold.

Run `tools/arena/bootstrap.sh --runtime` to verify hashes, reconstruct the
binary in `/tmp/godotbin`, extract SwiftShader, and create the
`godot-swiftshader` launcher.

The files were imported from `Creative36711/advanced-randomizer` commit
`cda1b7d9b12beefc75b8f7cd440b8dd239fec8ad`. Their exact hashes are recorded in
`SHA256SUMS`.
