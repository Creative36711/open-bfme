#!/usr/bin/env bash
# Recreate the Arena toolchain from files pinned in this repository.
#
#   tools/arena/bootstrap.sh                 # runtime + art tools
#   tools/arena/bootstrap.sh --runtime       # CMake/Ninja + Godot/SwiftShader
#   tools/arena/bootstrap.sh --art           # Godot + Pillow + bpy
#   tools/arena/bootstrap.sh --check         # inspect everything, install nothing
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
WORK="${WORK:-/tmp}"
PROFILE="all"
CHECK=0

usage() {
  sed -n '2,7p' "$0"
}
for argument in "$@"; do
  case "$argument" in
    --runtime) PROFILE="runtime" ;;
    --art) PROFILE="art" ;;
    --check) CHECK=1 ;;
    -h|--help) usage; exit 0 ;;
    *) printf 'Unknown option: %s\n' "$argument" >&2; usage >&2; exit 2 ;;
  esac
done

say()  { printf '==> %s\n' "$*"; }
ok()   { printf '    ✓ %s\n' "$*"; }
miss() { printf '    ✗ %s\n' "$*"; }
die()  { printf 'ERROR: %s\n' "$*" >&2; exit 1; }

want_runtime() { [ "$PROFILE" = "all" ] || [ "$PROFILE" = "runtime" ]; }
want_art()     { [ "$PROFILE" = "all" ] || [ "$PROFILE" = "art" ]; }

install_toolchain() {
  local prefix="$WORK/buildenv"
  if [ -x "$prefix/bin/cmake" ] && [ -x "$prefix/bin/ninja" ]; then
    ok "$($prefix/bin/cmake --version | head -1), ninja $($prefix/bin/ninja --version)"
    return
  fi
  if [ "$CHECK" = 1 ]; then miss "CMake/Ninja missing in $prefix"; return; fi
  say "CMake + Ninja -> $prefix"
  python3 -m venv "$prefix"
  "$prefix/bin/pip" -q install --disable-pip-version-check --progress-bar off cmake ninja
  ok "$($prefix/bin/cmake --version | head -1), ninja $($prefix/bin/ninja --version)"
}

GODOT_DIR="$WORK/godotbin"
GODOT_BIN="$GODOT_DIR/godot"
GODOT_RUN="$GODOT_DIR/godot-swiftshader"
GODOT_VENDOR="$REPO/tools/vendor/godot"

verify_godot_vendor() {
  python3 - "$GODOT_VENDOR" <<'PY'
import hashlib
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
checksums = {}
for line in (root / "SHA256SUMS").read_text(encoding="ascii").splitlines():
    if not line or line.startswith("#"):
        continue
    digest, name = line.split("  ", 1)
    checksums[name] = digest

def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()

for name in (
    "godot-linux-x86_64.xz.part-00",
    "godot-linux-x86_64.xz.part-01",
    "swiftshader-linux-x86_64.tar.xz",
):
    path = root / name
    actual = sha256(path)
    expected = checksums.get(name)
    if actual != expected:
        raise SystemExit(f"{name}: {actual} != {expected}")
print("vendor archives verified")
PY
}

write_godot_wrapper() {
  cat > "$GODOT_RUN" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export VK_ICD_FILENAMES="$HERE/vk_swiftshader_icd.json"
export LD_LIBRARY_PATH="$HERE${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$HERE/godot" "$@"
EOF
  chmod +x "$GODOT_RUN"
}

install_godot() {
  if [ -x "$GODOT_BIN" ] && [ -x "$GODOT_RUN" ] \
     && [ -f "$GODOT_DIR/vk_swiftshader_icd.json" ]; then
    ok "Godot: $($GODOT_BIN --version 2>/dev/null | tail -1)"
    return
  fi
  if [ "$CHECK" = 1 ]; then miss "Godot/SwiftShader missing in $GODOT_DIR"; return; fi
  [ -f "$GODOT_VENDOR/SHA256SUMS" ] || die "missing $GODOT_VENDOR/SHA256SUMS"
  say "Godot 4.7.2 + SwiftShader from the repository"
  verify_godot_vendor
  rm -rf "$GODOT_DIR"
  mkdir -p "$GODOT_DIR"
  cat "$GODOT_VENDOR/godot-linux-x86_64.xz.part-00" \
      "$GODOT_VENDOR/godot-linux-x86_64.xz.part-01" > "$GODOT_DIR/godot.xz"
  xz -d "$GODOT_DIR/godot.xz"
  chmod +x "$GODOT_BIN"
  tar -xJf "$GODOT_VENDOR/swiftshader-linux-x86_64.tar.xz" -C "$GODOT_DIR"
  python3 - "$GODOT_BIN" "$GODOT_VENDOR/SHA256SUMS" <<'PY'
import hashlib
import pathlib
import sys

binary = pathlib.Path(sys.argv[1])
checksums = pathlib.Path(sys.argv[2]).read_text(encoding="ascii").splitlines()
expected = next(line.split("  ", 1)[0] for line in checksums if line.endswith("  godot (uncompressed)"))
digest = hashlib.sha256()
with binary.open("rb") as stream:
    for block in iter(lambda: stream.read(1024 * 1024), b""):
        digest.update(block)
actual = digest.hexdigest()
if actual != expected:
    raise SystemExit(f"godot: {actual} != {expected}")
PY
  write_godot_wrapper
  ok "Godot: $($GODOT_BIN --version 2>/dev/null | tail -1)"
}

install_media() {
  local prefix="$WORK/mediaenv"
  local wrapper="$prefix/bin/cutout-chromakey"
  if [ -x "$wrapper" ] && "$prefix/bin/python" -c 'import PIL' 2>/dev/null; then
    ok "Pillow/chromakey: $wrapper"
    return
  fi
  if [ "$CHECK" = 1 ]; then miss "Pillow/chromakey missing in $prefix"; return; fi
  say "Pillow + chromakey -> $prefix"
  python3 -m venv "$prefix"
  "$prefix/bin/pip" -q install --disable-pip-version-check --progress-bar off Pillow
  cat > "$wrapper" <<EOF
#!/usr/bin/env bash
exec "$prefix/bin/python" "$REPO/tools/media/cutout_chromakey.py" "\$@"
EOF
  chmod +x "$wrapper"
  ok "Pillow/chromakey: $wrapper"
}

BPY_VERSION="5.0.1"
BPY_WHEEL_SHA256="79c6e421f53ea24a5961d86eefb8141798eee708682415eb4f5a952c2abdeb8b"

install_bpy() {
  local prefix="$WORK/bpyenv"
  local stubs="$WORK/bpystubs"
  local wrapper="$prefix/bin/bpy-python"
  if [ -x "$wrapper" ] && "$wrapper" -c 'import bpy' 2>/dev/null; then
    ok "Blender: $($wrapper -c 'import bpy; print(bpy.app.version_string)')"
    return
  fi
  if [ "$CHECK" = 1 ]; then miss "bpy $BPY_VERSION missing in $prefix"; return; fi
  command -v python3.11 >/dev/null || die "python3.11 is required for bpy $BPY_VERSION"
  say "Blender $BPY_VERSION as bpy (geometry/animation/GLB; rendering stays in Godot)"
  rm -rf "$prefix" "$stubs" "$WORK/bpywheel"
  python3.11 -m venv "$prefix"
  mkdir -p "$WORK/bpywheel"
  "$prefix/bin/pip" -q download --disable-pip-version-check --no-deps \
    -d "$WORK/bpywheel" "bpy==$BPY_VERSION"
  local wheel
  wheel=$(find "$WORK/bpywheel" -maxdepth 1 -name "bpy-$BPY_VERSION-*.whl" -print -quit)
  [ -n "$wheel" ] || die "bpy wheel was not downloaded"
  local actual
  actual=$(python3 - "$wheel" <<'PY'
import hashlib
import pathlib
import sys
h = hashlib.sha256()
with pathlib.Path(sys.argv[1]).open("rb") as stream:
    for block in iter(lambda: stream.read(1024 * 1024), b""):
        h.update(block)
print(h.hexdigest())
PY
)
  [ "$actual" = "$BPY_WHEEL_SHA256" ] \
    || die "bpy wheel hash mismatch: $actual != $BPY_WHEEL_SHA256"
  "$prefix/bin/pip" -q install --disable-pip-version-check --progress-bar off "$wheel"
  local package
  package=$(find "$prefix/lib" -type d -path '*/site-packages/bpy' -print -quit)
  [ -n "$package" ] || die "installed bpy package was not found"
  python3.11 "$HERE/bpy_stubs.py" "$package" "$stubs"
  cat > "$wrapper" <<EOF
#!/usr/bin/env bash
export LD_LIBRARY_PATH="$stubs\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
exec "$prefix/bin/python" "\$@"
EOF
  chmod +x "$wrapper"
  "$wrapper" -c 'import bpy' || die "bpy still cannot be imported after stubbing"
  rm -rf "$WORK/bpywheel"
  ok "Blender: $($wrapper -c 'import bpy; print(bpy.app.version_string)')"
}

if want_runtime; then install_toolchain; fi
install_godot
if want_art; then
  install_media
  install_bpy
fi
if [ "$CHECK" = 1 ]; then
  printf '\nCheck complete; no files were installed.\n'
  exit 0
fi

cat <<EOF

Ready:
  Godot (SwiftShader): $GODOT_RUN --path $REPO/godot
EOF
if want_runtime; then
  printf '  Build OpenBFME:      %s/tools/arena/build_engine.sh --quick\n' "$REPO"
fi
if want_art; then
  printf '  Chromakey:           %s/mediaenv/bin/cutout-chromakey in.png out.png --crop\n' "$WORK"
  printf '  Blender Python:      %s/bpyenv/bin/bpy-python script.py\n' "$WORK"
fi
