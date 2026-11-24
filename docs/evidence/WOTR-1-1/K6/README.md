# K6 — Arena build snapshot

The snapshot is a local iteration aid, not an acceptance result. Use a trusted external store; only restore at the same canonical checkout path with the same toolchain and the pinned godot-cpp already populated. Generate Godot imports before saving; move all three output directories aside before restoring. Do not run build/import concurrently with save or restore. No remote upload or compiler launcher is enabled by default.

```sh
JOBS=2 tools/arena/build_engine.sh --clean --tests
engine/build/openbfme_tests
python3 tools/arena/build_cache.py save /tmp/openbfme-build-cache
# Move engine/build, godot/bin and godot/.godot out of the checkout.
python3 tools/arena/build_cache.py restore /tmp/openbfme-build-cache
```

Record the exact commit, return codes, times, binary hashes and archive hash in the PR/run report (a commit cannot contain its own SHA). `--check` is informational; restore additionally requires the binaries and verifies their hashes. The published 754.83 s reference is a release Python test run, not engine compilation; compiler-cache impact on it cannot be inferred from a warm C++ build. Compiler-cache adoption requires a measured comparable end-to-end cycle saving at least 25%.
