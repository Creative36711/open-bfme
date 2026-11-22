#!/usr/bin/env bash
# WOTR-1.1-K4 (Godot World Engineer): run the automated route of the Remaster
# world - select -> move -> conflict -> battle launch -> return -> save/load -
# and write the screenshots and the report.
#
#   godot/wotr_remaster/scenario/run_scenario.sh [pack.json] [output dir]
#
# Environment:
#   GODOT_RUN   the Godot binary (default /tmp/godotbin/godot-swiftshader)
#   PACK        the campaign pack (default campaigns/fixtures/wotr_1_1/pack.json)
#   OUT         the artifact directory (default /tmp/wotr-k4)
#   SEED        the campaign seed (default 20261121)
#   RESOLUTION  the render size (default 1280x720)
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../../.." && pwd)"

GODOT_RUN="${GODOT_RUN:-/tmp/godotbin/godot-swiftshader}"
PACK="${1:-${PACK:-$REPO/campaigns/fixtures/wotr_1_1/pack.json}}"
OUT="${2:-${OUT:-${TMPDIR:-/tmp}/wotr-k4}}"
SEED="${SEED:-20261121}"
RESOLUTION="${RESOLUTION:-1280x720}"

[ -x "$GODOT_RUN" ] || { echo "Godot is missing: run tools/arena/bootstrap.sh --runtime" >&2; exit 1; }
[ -f "$PACK" ] || { echo "the campaign pack is missing: $PACK" >&2; exit 1; }
mkdir -p "$OUT"

echo "==> build the native core extension"
bash "$REPO/godot/wotr_remaster/core/native/build_core_extension.sh"

echo "==> import the project (registers the extension)"
# The first import writes the extension list; the second one sees it.
for attempt in 1 2; do
    "$GODOT_RUN" --headless --path "$REPO/godot" --import > "$OUT/import-$attempt.log" 2>&1 || true
    [ -f "$REPO/godot/.godot/extension_list.cfg" ] && break
done
if [ ! -f "$REPO/godot/.godot/extension_list.cfg" ]; then
    cat "$OUT"/import-*.log >&2
    echo "Godot did not register the extensions" >&2
    exit 1
fi
grep -q "wotr_core" "$REPO/godot/.godot/extension_list.cfg" \
    || { echo "the wotr_core extension is not registered" >&2; cat "$REPO/godot/.godot/extension_list.cfg" >&2; exit 1; }

echo "==> run the route"
set +e
"$GODOT_RUN" --display-driver offscreen --rendering-driver vulkan \
    --path "$REPO/godot" --resolution "$RESOLUTION" \
    --script res://wotr_remaster/scenario/wotr_headless_scenario.gd \
    -- --pack="$PACK" --out="$OUT" --seed="$SEED" > "$OUT/run.log" 2>&1 &
godot_pid=$!

# The peak resident set of the whole game process (the retail archives are
# mapped into it). The kernel keeps the high-water mark in /proc/<pid>/status
# (VmHWM), so the last sample of the run is the peak; the sample is taken
# here, in the shell, because the shipped GDScript starts no process.
peak_rss_kb=0
while kill -0 "$godot_pid" 2>/dev/null; do
    hwm=$(awk '/^VmHWM:/{print $2}' "/proc/$godot_pid/status" 2>/dev/null)
    if [ -n "$hwm" ] && [ "$hwm" -gt "$peak_rss_kb" ]; then
        peak_rss_kb=$hwm
    fi
    sleep 0.2
done
wait "$godot_pid"
status=$?
set -e

cat "$OUT/run.log"

if [ -f "$OUT/report.json" ] && [ "$peak_rss_kb" -gt 0 ]; then
    python3 - "$OUT/report.json" "$peak_rss_kb" <<'PYEOF'
import json, sys
path, peak = sys.argv[1], int(sys.argv[2])
with open(path, encoding="utf-8") as handle:
    report = json.load(handle)
memory = report.setdefault("memory", {})
memory["peak_rss_kb"] = peak
with open(path, "w", encoding="utf-8") as handle:
    json.dump(report, handle, indent=2)
    handle.write("\n")
PYEOF
fi

echo "==> rc=$status"
echo "==> report: $OUT/report.json"
exit "$status"
