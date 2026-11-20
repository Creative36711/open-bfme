#!/usr/bin/env bash
# Configure and build the OpenBFME C++ layer in Arena.
#
#   tools/arena/build_engine.sh             # runnable GDExtension, tests off
#   tools/arena/build_engine.sh --quick     # smallest edit/compile loop
#   tools/arena/build_engine.sh --core      # openbfme_core only
#   tools/arena/build_engine.sh --tests     # extension + C++ tests
#   tools/arena/build_engine.sh --check     # inspect outputs only
#   tools/arena/build_engine.sh --clean     # clean runnable build
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
BUILD="$REPO/engine/build"
PATH="/tmp/buildenv/bin:$PATH"
MODE="runtime"
CLEAN=0

usage() { sed -n '2,9p' "$0"; }
for argument in "$@"; do
  case "$argument" in
    --quick) MODE="quick" ;;
    --core) MODE="core" ;;
    --tests) MODE="tests" ;;
    --check) MODE="check" ;;
    --clean) CLEAN=1 ;;
    -h|--help) usage; exit 0 ;;
    *) printf 'Unknown option: %s\n' "$argument" >&2; usage >&2; exit 2 ;;
  esac
done

say() { printf '==> %s\n' "$*"; }
ok()  { printf '    ✓ %s\n' "$*"; }
die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }

if [ "$MODE" = "check" ]; then
  printf 'source: %s\nbuild:  %s\n' "$REPO" "$BUILD"
  [ -d "$REPO/engine/thirdparty/godot-cpp/src" ] \
    && ok "godot-cpp present" || printf '    ✗ godot-cpp not populated\n'
  [ -f "$BUILD/libopenbfme_core.a" ] \
    && ok "core: $(du -h "$BUILD/libopenbfme_core.a" | cut -f1)" \
    || printf '    ✗ core not built\n'
  extension=$(find "$REPO/godot/bin" -maxdepth 1 -type f \
    \( -name 'libopenbfme.so' -o -name 'openbfme.linux*' \) -print -quit 2>/dev/null || true)
  [ -n "$extension" ] && ok "extension: $extension" || printf '    ✗ extension not built\n'
  [ -x "$BUILD/openbfme_tests" ] \
    && ok "tests: $BUILD/openbfme_tests" || printf '    ✗ tests not built\n'
  exit 0
fi

command -v cmake >/dev/null || die "CMake missing; run tools/arena/bootstrap.sh --runtime"
command -v ninja >/dev/null || die "Ninja missing; run tools/arena/bootstrap.sh --runtime"
command -v curl >/dev/null || die "curl missing"
command -v unzip >/dev/null || die "unzip missing"
[ -f "$REPO/engine/CMakeLists.txt" ] \
  || die "OpenBFME source is not imported at repository root yet"

# The public upstream stores godot-cpp as a submodule. Arena checkouts do not
# necessarily populate it, while codeload.github.com is reachable. Resolve the
# exact gitlink from the checked-out upstream commit instead of hard-coding a
# version, so nightly merges remain reproducible.
GODOTCPP_REV="${GODOTCPP_REV:-$(git -C "$REPO" ls-tree HEAD engine/thirdparty/godot-cpp | awk '{print $3}')}"
[ -n "$GODOTCPP_REV" ] || die "cannot resolve the godot-cpp gitlink"
if [ ! -d "$REPO/engine/thirdparty/godot-cpp/src" ]; then
  say "godot-cpp $GODOTCPP_REV"
  temporary=$(mktemp -d)
  trap 'rm -rf "$temporary"' EXIT
  curl -fsSL "https://codeload.github.com/godotengine/godot-cpp/zip/$GODOTCPP_REV" \
    -o "$temporary/godot-cpp.zip"
  unzip -q "$temporary/godot-cpp.zip" -d "$temporary/unpacked"
  rm -rf "$REPO/engine/thirdparty/godot-cpp"
  mv "$temporary"/unpacked/* "$REPO/engine/thirdparty/godot-cpp"
  rm -rf "$temporary"
  trap - EXIT
  ok "godot-cpp populated"
fi

[ "$CLEAN" = 0 ] || { say "remove $BUILD"; rm -rf "$BUILD"; }
mkdir -p "$BUILD"

GODOT=ON
TESTS=OFF
TARGET=openbfme
case "$MODE" in
  quick) GODOT=OFF; TARGET=openbfme_peer ;;
  core) GODOT=OFF; TARGET=openbfme_core ;;
  tests) TESTS=ON; TARGET="openbfme openbfme_tests" ;;
esac

say "configure (mode=$MODE, Godot=$GODOT, tests=$TESTS)"
if ! cmake -S "$REPO/engine" -B "$BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE:-Release}" \
    -DOPENBFME_BUILD_GODOT="$GODOT" \
    -DOPENBFME_BUILD_TESTS="$TESTS" \
    -DCMAKE_CXX_FLAGS="${OPENBFME_EXTRA_CXX_FLAGS:--include cstddef}" \
    > "$BUILD/configure.log" 2>&1; then
  tail -40 "$BUILD/configure.log" >&2
  die "configuration failed; see $BUILD/configure.log"
fi
ok "configuration complete"

JOBS="${JOBS:-$(nproc)}"
say "build $TARGET (-j$JOBS)"
start=$(date +%s)
# TARGET intentionally expands to two target arguments in --tests mode.
# shellcheck disable=SC2086
if ! ninja -C "$BUILD" $TARGET -j"$JOBS" > "$BUILD/build.log" 2>&1; then
  grep -m20 'error:' "$BUILD/build.log" >&2 || tail -40 "$BUILD/build.log" >&2
  die "build failed; see $BUILD/build.log"
fi
elapsed=$(( $(date +%s) - start ))
printf '    elapsed: %d min %d s\n' $((elapsed / 60)) $((elapsed % 60))
ok "built: $TARGET"
find "$REPO/godot/bin" -maxdepth 1 -type f -name '*.so' -printf '    extension: %p (%k KiB)\n' 2>/dev/null || true
