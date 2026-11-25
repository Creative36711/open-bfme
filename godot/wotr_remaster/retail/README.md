# WOTR Remaster · retail pointer (card WOTR-1.1-K7)

The Remaster's own pointer asset and its registration in a retail install.

## The open path this closes

The retail Godot smoke (`tools/arena/smoke_godot.sh --retail`) stopped with

```text
ERROR: cursor Arrow: no file SCCPointer.cur
```

The engine resolves the pointer images in exactly one place: it reads
`data/ini/mouse.ini` out of the mounted archives (`GodotMouse::loadCursorTable`)
and looks the image that Mouse.ini names up in `<install>/data/cursors`, case
insensitively (`FindFileNoCase`). The retail installs ship those images as loose
files (`data/cursors/SCCPointer.cur`, the Arrow cursor, hot spot `X:2 Y:2`).

The pinned retail snapshot is the archive-only transfer
(`openbfme.retail-big-transfer`): it carries the 213 BIG archives and not the
loose files of the install root, so the pointer is not in it - and no retail
byte may enter the repository.

## What is here

```text
godot/wotr_remaster/retail/
├── wotr_pointer_registry.gd   the asset and the registration
├── wotr_pointer_stage.gd       the command that registers it in an install root
└── run_retail_smoke.sh         the runner: the install overlay and the smoke
```

* **The asset** is the Remaster's own 32×32 arrow, built from the ASCII source
  in `wotr_pointer_registry.gd` into a Windows cursor file (4,286 bytes, the
  shape `DecodeCursorFile` reads: CURSORDIR, CURSORDIRENTRY with the hot spot
  at (2, 2), a 32-bit BITMAPINFOHEADER image and its AND mask). It is not a
  retail copy and it is not derived from one.
* **The registration** writes that asset into `<install>/data/cursors/SCCPointer.cur`:
  the path and the name the engine resolves. It is idempotent: an install that
  already carries the file keeps it, so a complete retail install keeps its own
  pointer.
* **The runner** mirrors the pinned retail trees into an install overlay (the
  same archives, linked; the pinned snapshot stays the only copy of the retail
  bytes), registers the pointer in the overlay and runs the Arena smoke
  unmodified against it.

## The commands

```bash
# the registration on its own
godot --headless --path godot \
  --script res://wotr_remaster/retail/wotr_pointer_stage.gd \
  -- --install=<install root>

# the smoke with retail paths (BFME2_INSTALL / ROTWK_INSTALL name the trees)
godot/wotr_remaster/retail/run_retail_smoke.sh
```

`run_retail_smoke.sh --no-pointer` runs the same smoke with the pointer left
out, which reproduces the run that used to stop on the missing asset.
