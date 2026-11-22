#!/usr/bin/env bash
# WOTR-1.1-K4 (Godot World Engineer): build the Godot binding of the
# WotrRemaster strategic core.
#
#   godot/wotr_remaster/core/native/build_core_extension.sh
#
# The strategic core comes from its canonical location
# (engine/src/GameLogic/WotrRemaster) and is not modified. The result is a
# GDExtension in bin/ next to this script; `build/` and `bin/` are ignored
# build outputs.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../../../.." && pwd)"

# tools/arena/bootstrap.sh --runtime installs cmake and ninja into /tmp/buildenv.
if [ -x "${WORK:-/tmp}/buildenv/bin/cmake" ]; then
    PATH="${WORK:-/tmp}/buildenv/bin:$PATH"
fi
export PATH

command -v cmake >/dev/null || { echo "cmake missing; run tools/arena/bootstrap.sh --runtime" >&2; exit 1; }

cmake -S "$HERE" -B "$HERE/build" -G Ninja -DCMAKE_BUILD_TYPE="${BUILD_TYPE:-Release}"
cmake --build "$HERE/build" --target wotr_core

ls -l "$HERE/bin"
