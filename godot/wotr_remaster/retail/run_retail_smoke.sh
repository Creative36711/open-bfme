#!/usr/bin/env bash
# WOTR-1.1-K7: run the Arena Godot smoke with retail paths on the pinned retail
# snapshot, with the Remaster's pointer registered in the install path.
#
#   godot/wotr_remaster/retail/run_retail_smoke.sh [--baseline] [--out=<dir>]
#
# Environment:
#   BFME2_INSTALL  the pinned retail BFME2 tree (default $REPO/.retail/BFME2)
#   ROTWK_INSTALL  the pinned retail RotWK tree (default $REPO/.retail/RotWK)
#   GODOT_RUN      the Godot binary (default /tmp/godotbin/godot-swiftshader)
#
# The retail trees are read-only inputs. The runner mirrors them into an
# install overlay (the same archives, linked; the pinned snapshot stays the
# only copy of the retail bytes) and registers the Remaster's pointer in the
# overlay's `data/cursors`, so the engine resolves the pointer through the
# retail path without a byte of the pinned snapshot changing.
#
# The check is the Arena smoke itself (`tools/arena/smoke_godot.sh --retail`),
# unmodified: the runner only prepares the install paths it sees and measures
# the run. `--baseline` adds one more smoke run of the same oracle with the
# pointer left out, which is the run that used to stop on
# "cursor Arrow: no file SCCPointer.cur"; the acceptance run is the one with
# the pointer, and it is the only run without `--baseline`.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../../.." && pwd)"

GODOT_RUN="${GODOT_RUN:-/tmp/godotbin/godot-swiftshader}"
BFME2_INSTALL="${BFME2_INSTALL:-$REPO/.retail/BFME2}"
ROTWK_INSTALL="${ROTWK_INSTALL:-$REPO/.retail/RotWK}"
OUT="${OUT:-${TMPDIR:-/tmp}/wotr-k7}"
POINTER=1
BASELINE=0

for argument in "$@"; do
	case "$argument" in
		--no-pointer) POINTER=0 ;;
		--baseline) BASELINE=1 ;;
		--out=*) OUT="${argument#--out=}" ;;
		-h|--help) sed -n '2,22p' "$0"; exit 0 ;;
		*) printf 'Unknown option: %s\n' "$argument" >&2; exit 2 ;;
	esac
done

[ -x "$GODOT_RUN" ] || { echo "Godot is missing: run tools/arena/bootstrap.sh --runtime" >&2; exit 1; }
[ -d "$BFME2_INSTALL" ] || { echo "the retail BFME2 tree is missing: $BFME2_INSTALL" >&2; exit 1; }
[ -d "$ROTWK_INSTALL" ] || { echo "the retail RotWK tree is missing: $ROTWK_INSTALL" >&2; exit 1; }
[ -f "$REPO/tools/arena/smoke_godot.sh" ] || { echo "the Arena smoke is missing" >&2; exit 1; }

mkdir -p "$OUT"

# The overlay: the retail tree's own files, linked, in the tree's own layout.
# The directories are real (the mount walks them), the files are links (the
# pinned snapshot stays the only copy of the retail bytes).
mirror() {
	local src="$1" dst="$2" entry name
	mkdir -p "$dst"
	for entry in "$src"/*; do
		[ -e "$entry" ] || continue
		name="$(basename "$entry")"
		if [ -d "$entry" ]; then
			mirror "$entry" "$dst/$name"
		else
			ln -sfn "$entry" "$dst/$name"
		fi
	done
}

echo "==> build the install overlay"
rm -rf "$OUT/install"
mirror "$BFME2_INSTALL" "$OUT/install/BFME2"
mirror "$ROTWK_INSTALL" "$OUT/install/RotWK"
BFME2_ROOT="$OUT/install/BFME2"
ROTWK_ROOT="$OUT/install/RotWK"

register_pointer() {
	"$GODOT_RUN" --headless --path "$OUT/project" \
		--script res://wotr_remaster/retail/wotr_pointer_stage.gd \
		-- "--install=$BFME2_ROOT" "--install=$ROTWK_ROOT" \
		"--preview=$OUT/pointer.png" "--preview-scale=16" \
		> "$OUT/stage.log" 2>&1
}

if [ "$POINTER" = 1 ]; then
	# The stage runs in the same project the smoke builds: a copy of the Godot
	# project, imported once. The import registers the GDExtension, so the stage
	# runs with the extension loaded, exactly as the smoke does; the checkout
	# itself is never written to.
	echo "==> stage the Godot project (the copy the smoke builds)"
	rm -rf "$OUT/project"
	mkdir -p "$OUT/project"
	(cd "$REPO/godot" && tar --exclude='./.godot' -cf - .) | tar -xf - -C "$OUT/project"
	"$GODOT_RUN" --headless --path "$OUT/project" --import > "$OUT/import.log" 2>&1 || true

	echo "==> register the Remaster's pointer in the install path"
	if ! register_pointer; then
		cat "$OUT/stage.log" >&2
		echo "the pointer could not be registered" >&2
		exit 1
	fi
	grep -E "WOTR-K7 STAGE" "$OUT/stage.log" || true
else
	echo "==> the pointer is left out (--no-pointer)"
	: > "$OUT/stage.log"
fi

# The acceptance run: the Arena smoke, unmodified, with retail paths.
run_smoke() {
	local log="$1"
	set +e
	local start end
	start=$(date +%s.%N)
	BFME2_INSTALL="$BFME2_ROOT" ROTWK_INSTALL="$ROTWK_ROOT" \
		bash "$REPO/tools/arena/smoke_godot.sh" --retail > "$log" 2>&1
	SMOKE_RC=$?
	end=$(date +%s.%N)
	set -e
	SMOKE_DURATION=$(python3 -c "import sys; print('%.3f' % (float(sys.argv[2]) - float(sys.argv[1])))" "$start" "$end")
}

echo "==> run the Arena Godot smoke with retail paths"
run_smoke "$OUT/smoke.log"
rc=$SMOKE_RC
duration=$SMOKE_DURATION

echo "==> rc=$rc duration=${duration}s"
grep -E "SMOKE (PASS|FAIL)|cursor Arrow|OpenBFME v" "$OUT/smoke.log" | head -20 || true

baseline_rc=""
baseline_duration=""
if [ "$BASELINE" = 1 ]; then
	echo "==> baseline: the same smoke with the pointer left out"
	rm -f "$BFME2_ROOT/data/cursors/SCCPointer.cur" "$ROTWK_ROOT/data/cursors/SCCPointer.cur"
	run_smoke "$OUT/smoke-baseline.log"
	baseline_rc=$SMOKE_RC
	baseline_duration=$SMOKE_DURATION
	echo "==> baseline rc=$baseline_rc duration=${baseline_duration}s"
	grep -E "SMOKE (PASS|FAIL)|cursor Arrow|OpenBFME v" "$OUT/smoke-baseline.log" | head -20 || true
fi

python3 - "$OUT/smoke.json" "$rc" "$duration" "$BFME2_ROOT" "$ROTWK_ROOT" "$POINTER" \
	"$baseline_rc" "$baseline_duration" "$OUT/smoke.log" "$OUT/smoke-baseline.log" <<'PYEOF'
import json, os, sys
(path, rc, duration, bfme2, rotwk, pointer, baseline_rc, baseline_duration,
 smoke_log, baseline_log) = sys.argv[1:11]

def cursor_error(text):
    return "cursor Arrow: no file SCCPointer.cur" in text

def read(name):
    if not os.path.exists(name):
        return ""
    with open(name, encoding="utf-8", errors="replace") as handle:
        return handle.read()

report = {
    "smoke": "tools/arena/smoke_godot.sh --retail",
    "rc": int(rc),
    "duration_seconds": float(duration),
    "pointer_registered": pointer == "1",
    "install_roots": {"BFME2_INSTALL": bfme2, "ROTWK_INSTALL": rotwk},
    "log": "smoke.log",
    "cursor_error": cursor_error(read(smoke_log)),
}
if baseline_rc != "":
    report["baseline"] = {
        "rc": int(baseline_rc),
        "duration_seconds": float(baseline_duration),
        "log": "smoke-baseline.log",
        "cursor_error": cursor_error(read(baseline_log)),
    }
with open(path, "w", encoding="utf-8") as handle:
    json.dump(report, handle, indent=2)
    handle.write("\n")
print("WOTR-K7 SMOKE ", json.dumps(report))
PYEOF

exit "$rc"
