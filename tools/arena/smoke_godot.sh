#!/usr/bin/env bash
# Verify that Godot loads the freshly built OpenBFME GDExtension and scripts.
# Source-only mode treats the expected "no game folders" stop as success.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
GODOT_RUN="${GODOT_RUN:-/tmp/godotbin/godot-swiftshader}"
MODE="source-only"
[ "${1:-}" = "--retail" ] && MODE="retail"
[ $# -le 1 ] || { echo "usage: $0 [--retail]" >&2; exit 2; }

[ -x "$GODOT_RUN" ] || {
  echo "Godot wrapper missing; run tools/arena/bootstrap.sh --runtime" >&2
  exit 1
}
[ -f "$REPO/godot/bin/openbfme.linux.template_debug.x86_64.so" ] || {
  echo "OpenBFME extension missing; run tools/arena/build_engine.sh" >&2
  exit 1
}

# Godot writes missing .uid/.import sidecars beside sources during import. Stage
# the small project in /tmp so a smoke test never dirties or mutates the checkout.
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
project="$work/project"
mkdir -p "$project"
(
  cd "$REPO/godot"
  tar --exclude='./.godot' -cf - .
) | tar -xf - -C "$project"

# A fresh project has no .godot/extension_list.cfg. Upstream permits two import
# attempts because the first can stop during initial extension registration.
for attempt in 1 2; do
  [ -f "$project/.godot/extension_list.cfg" ] && break
  "$GODOT_RUN" --headless --path "$project" --import \
    > "$work/import-$attempt.log" 2>&1 || true
done
if [ ! -f "$project/.godot/extension_list.cfg" ]; then
  cat "$work"/import-*.log >&2
  echo "Godot did not register openbfme.gdextension" >&2
  exit 1
fi

log="$work/godot-smoke.log"
set +e
if [ "$MODE" = "source-only" ]; then
  mkdir -p "$work/home" "$work/data" "$work/config" "$work/cache"
  env -u BFME2_INSTALL -u ROTWK_INSTALL \
    HOME="$work/home" XDG_DATA_HOME="$work/data" \
    XDG_CONFIG_HOME="$work/config" XDG_CACHE_HOME="$work/cache" \
    "$GODOT_RUN" --headless --path "$project" --quit-after 5 \
    > "$log" 2>&1
else
  : "${BFME2_INSTALL:?set BFME2_INSTALL for --retail}"
  : "${ROTWK_INSTALL:?set ROTWK_INSTALL for --retail}"
  "$GODOT_RUN" --headless --path "$project" --quit-after 5 \
    > "$log" 2>&1
fi
status=$?
set -e
cat "$log"

if grep -q 'the openbfme extension is not loaded' "$log"; then
  echo "SMOKE FAIL: GDExtension was not loaded" >&2
  exit 1
fi
if grep -q 'SCRIPT ERROR: Parse Error' "$log"; then
  echo "SMOKE FAIL: GDScript parse error" >&2
  exit 1
fi
if ! grep -Eq '^OpenBFME (v[0-9][^ ]*|[0-9a-f]{10}(-dirty)?)( |$)' "$log"; then
  echo "SMOKE FAIL: native OpenBFME version banner is absent" >&2
  exit 1
fi

if [ "$MODE" = "source-only" ]; then
  if ! grep -q 'no game folders chosen yet' "$log"; then
    echo "SMOKE FAIL: expected the clean no-retail stop" >&2
    exit 1
  fi
  echo "SMOKE PASS: extension and scripts loaded; retail intentionally absent"
  exit 0
fi

[ "$status" = 0 ] || {
  echo "SMOKE FAIL: Godot exited $status with retail paths configured" >&2
  exit "$status"
}
echo "SMOKE PASS: extension and scripts loaded with retail paths"
