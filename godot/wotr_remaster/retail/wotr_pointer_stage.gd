extends SceneTree
## WOTR-1.1-K7: registers the Remaster's pointer in the retail install roots.
##
##   godot --path godot --script res://wotr_remaster/retail/wotr_pointer_stage.gd -- --install=<root> [--install=<root>]
##
## The registration is the Remaster's own: the pointer asset is written into
## `<root>/data/cursors/SCCPointer.cur`, the path and the name the engine
## resolves (`GodotMouse::loadCursorTable` → `FindFileNoCase`). An install
## that already carries the file keeps it.
##
## The script prints one JSON line with what it registered and exits 0; a
## failure exits 1.

const Registry := preload("res://wotr_remaster/retail/wotr_pointer_registry.gd")


func _initialize() -> void:
	var roots := _install_roots()
	if roots.is_empty():
		print("WOTR-K7 STAGE FAIL: no --install=<root> was given")
		quit(1)
		return
	var results: Array = []
	var failures := 0
	for root in roots:
		var result: Dictionary = Registry.register_into(str(root))
		if not bool(result.get("ok", false)):
			failures += 1
		results.append(result)
	var preview := _preview_path()
	if not preview.is_empty():
		var image: Image = Registry.preview_image()
		var scale := _preview_scale()
		if scale > 1:
			image.resize(image.get_width() * scale, image.get_height() * scale, Image.INTERPOLATE_NEAREST)
		var error := image.save_png(preview)
		if error != OK:
			failures += 1
			print("WOTR-K7 STAGE: the preview could not be written: ", error_string(error))
	var report := {
		"asset_sha256": Registry.cursor_sha256(),
		"asset_bytes": Registry.cursor_bytes().size(),
		"cursor_dir": Registry.CURSOR_DIR,
		"cursor_file": Registry.CURSOR_FILE,
		"preview": preview,
		"results": results,
		"result": "pass" if failures == 0 else "fail",
	}
	print("WOTR-K7 STAGE ", JSON.stringify(report))
	quit(0 if failures == 0 else 1)


func _install_roots() -> Array:
	var roots: Array = []
	for raw in OS.get_cmdline_user_args():
		var text := str(raw)
		if not text.begins_with("--install="):
			continue
		var root := text.substr(10)
		if not root.is_empty():
			roots.append(root)
	return roots


func _preview_path() -> String:
	for raw in OS.get_cmdline_user_args():
		var text := str(raw)
		if not text.begins_with("--preview="):
			continue
		var path := text.substr(10)
		if not path.is_empty():
			return path
	return ""


## The factor the written preview is scaled by, so that the 32x32 pointer can
## be looked at; 1 writes it at its own size.
func _preview_scale() -> int:
	for raw in OS.get_cmdline_user_args():
		var text := str(raw)
		if not text.begins_with("--preview-scale="):
			continue
		var value := text.substr(16).to_int()
		if value > 1:
			return value
	return 1
