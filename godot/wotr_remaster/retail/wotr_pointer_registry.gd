extends RefCounted
## WOTR-1.1-K7: the Remaster's pointer asset and its registration in a retail install.
##
## The retail installs ship their pointer images as the loose files of
## `<install>/data/cursors` that `data/ini/mouse.ini` names: the Arrow cursor
## is `SCCPointer.cur` with the hot spot `X:2 Y:2` (BFME2 1.06 and RotWK 2.01
## write the same name). The engine resolves them exactly there and nowhere
## else: `GodotMouse::loadCursorTable` reads `data/ini/mouse.ini` out of the
## mounted archives and looks the image up in `<install>/data/cursors`
## (case insensitively, `FindFileNoCase`), and a missing file is reported as
## "cursor Arrow: no file SCCPointer.cur".
##
## The pinned retail snapshot is the archive-only transfer
## (`openbfme.retail-big-transfer`): it carries the 213 BIG archives and not
## the loose files of the install root, so the pointer is not in it, and no
## retail byte may enter the repository.
##
## So the Remaster owns the pointer. This registry holds the asset - the
## Remaster's own 32x32 arrow, built from the ASCII source below (it is not a
## retail copy and not derived from one) - and registers it at the path the
## engine resolves, so that the engine finds the pointer through the retail
## path, in the same place and under the same name a complete install has it.
##
## The registration is idempotent and never replaces a file the install
## already has: a complete retail install keeps its own pointer.

const CURSOR_DIR := "data/cursors"
const CURSOR_FILE := "SCCPointer.cur"
const CURSOR_SIZE := 32
const HOTSPOT_X := 2
const HOTSPOT_Y := 2

## The Remaster's arrow: 'W' is the white body, 'K' the black outline, '.' is
## transparent. The tip sits at (2, 2), the hot spot Mouse.ini gives.
const ART := [
	"................................",
	"................................",
	"..K.............................",
	"..KK............................",
	"..KWK...........................",
	"..KWWKK.........................",
	"..KWWWWK........................",
	"..KWWWWWK........................",
	"..KWWWWWWKK......................",
	"..KWWWWWWWWK.....................",
	"..KWWWWWWWWWK....................",
	"..KWWWWWWWWWWKK..................",
	"..KWWWWWWWWWWWWK.................",
	"..KWWWWWWWKKKKKKK...............",
	"..KWWWWWWK......................",
	"..KWWWKWWWK.....................",
	"..KWWK.KWWWK....................",
	"..KWK...KWWK....................",
	"..KK.....KWWK...................",
	"..K......KWWK...................",
	"..........KWWK..................",
	"..........KWWWK.................",
	"...........KWK..................",
	"............K...................",
	"................................",
	"................................",
	"................................",
	"................................",
	"................................",
	"................................",
	"................................",
	"................................",
]


## The pointer as the engine reads it: a Windows cursor file (CURSORDIR +
## CURSORDIRENTRY + a 32-bit BITMAPINFOHEADER image and its AND mask), the
## shape `DecodeCursorFile` accepts.
static func cursor_bytes() -> PackedByteArray:
	var pixels := _pixels()
	var xor_size := CURSOR_SIZE * CURSOR_SIZE * 4
	var and_stride := ((CURSOR_SIZE + 31) / 32) * 4
	var image_size := 40 + xor_size + and_stride * CURSOR_SIZE
	var out := PackedByteArray()
	out.resize(22 + image_size)

	# CURSORDIR: reserved, type 2 (icon), one image.
	out.encode_u16(0, 0)
	out.encode_u16(2, 2)
	out.encode_u16(4, 1)
	# CURSORDIRENTRY: width, height, colours, reserved, hot spot x, hot spot y,
	# the image's size, the image's offset.
	out.encode_u8(6, CURSOR_SIZE)
	out.encode_u8(7, CURSOR_SIZE)
	out.encode_u8(8, 0)
	out.encode_u8(9, 0)
	out.encode_u16(10, HOTSPOT_X)
	out.encode_u16(12, HOTSPOT_Y)
	out.encode_u32(14, image_size)
	out.encode_u32(18, 22)

	# BITMAPINFOHEADER: the height is doubled (the image is stored bottom up)
	# and the image size is zero, as the engine's decoder requires.
	out.encode_u32(22, 40)
	out.encode_s32(26, CURSOR_SIZE)
	out.encode_s32(30, CURSOR_SIZE * 2)
	out.encode_u16(34, 1)
	out.encode_u16(36, 32)
	out.encode_u32(38, 0)
	out.encode_u32(42, 0)
	out.encode_s32(46, 0)
	out.encode_s32(50, 0)
	out.encode_u32(54, 0)
	out.encode_u32(58, 0)

	# The XOR bitmap: BGRA, bottom up. The AND mask stays zero; the alpha
	# carries the transparency.
	for y in range(CURSOR_SIZE - 1, -1, -1):
		for x in range(CURSOR_SIZE):
			var pixel: Array = pixels[y][x]
			var at := 62 + (CURSOR_SIZE - 1 - y) * CURSOR_SIZE * 4 + x * 4
			out.encode_u8(at, pixel[2])
			out.encode_u8(at + 1, pixel[1])
			out.encode_u8(at + 2, pixel[0])
			out.encode_u8(at + 3, pixel[3])
	return out


## The SHA-256 of the asset, so a report can name the exact bytes it staged.
static func cursor_sha256() -> String:
	var context := HashingContext.new()
	context.start(HashingContext.HASH_SHA256)
	context.update(cursor_bytes())
	return context.finish().hex_encode()


## The asset as an image, so that a person can see the pointer that is
## registered (the PNG the runner writes next to its report).
static func preview_image() -> Image:
	var pixels := _pixels()
	var image := Image.create_empty(CURSOR_SIZE, CURSOR_SIZE, false, Image.FORMAT_RGBA8)
	for y in range(CURSOR_SIZE):
		for x in range(CURSOR_SIZE):
			var pixel: Array = pixels[y][x]
			image.set_pixel(x, y, Color(
				float(pixel[0]) / 255.0, float(pixel[1]) / 255.0,
				float(pixel[2]) / 255.0, float(pixel[3]) / 255.0))
	return image


## The file of `dir` whose name equals `name` ignoring case, the way the
## engine's `FindFileNoCase` looks the pointer up; "" when there is none.
static func find_cursor(dir: String, name: String) -> String:
	var wanted := name.to_lower()
	var listing := DirAccess.open(dir)
	if listing == null:
		return ""
	for file_name in listing.get_files():
		if file_name.to_lower() == wanted:
			return dir.path_join(file_name)
	return ""


## Registers the pointer in the install root: the file lands in
## `<install_root>/data/cursors/SCCPointer.cur`, the path and the name the
## engine resolves. An install that already has the file keeps it.
static func register_into(install_root: String) -> Dictionary:
	if install_root.is_empty():
		return {"ok": false, "staged": false, "error": "no install root was given"}
	var dir := install_root.path_join(CURSOR_DIR)
	var existing := find_cursor(dir, CURSOR_FILE)
	if not existing.is_empty():
		return {
			"ok": true,
			"staged": false,
			"path": existing,
			"reason": "the install already has the pointer; it is kept",
		}
	var error := DirAccess.make_dir_recursive_absolute(dir)
	if error != OK:
		return {"ok": false, "staged": false, "error": "cannot create %s: %s" % [dir, error_string(error)]}
	var bytes := cursor_bytes()
	var file := FileAccess.open(dir.path_join(CURSOR_FILE), FileAccess.WRITE)
	if file == null:
		return {"ok": false, "staged": false, "error": "cannot write the pointer into %s" % dir}
	file.store_buffer(bytes)
	file.close()
	var written := FileAccess.open(dir.path_join(CURSOR_FILE), FileAccess.READ)
	if written == null:
		return {"ok": false, "staged": false, "error": "the pointer was written but cannot be read back"}
	var read_bytes := written.get_buffer(written.get_length())
	written.close()
	if read_bytes != bytes:
		return {"ok": false, "staged": false, "error": "the pointer written differs from the asset"}
	return {
		"ok": true,
		"staged": true,
		"path": dir.path_join(CURSOR_FILE),
		"bytes": bytes.size(),
		"sha256": cursor_sha256(),
		"reason": "the Remaster's pointer is registered in the install path",
	}


static func _pixels() -> Array:
	var pixels: Array = []
	for y in range(ART.size()):
		var row: Array = []
		var line: String = ART[y]
		for x in range(CURSOR_SIZE):
			var mark := line.substr(x, 1)
			match mark:
				"W":
					row.append([255, 255, 255, 255])
				"K":
					row.append([0, 0, 0, 255])
				_:
					row.append([0, 0, 0, 0])
		pixels.append(row)
	return pixels
