extends SceneTree
## WOTR-1.1-K4: the automated route through the Remaster world.
##
## select → move → conflict → battle launch → return → save/load, with the
## visual artifacts of the tiny map before and after the battle and the
## measured frame time and peak memory.
##
##   godot --path godot --script res://wotr_remaster/scenario/wotr_headless_scenario.gd -- --pack=<file> --out=<dir> [--seed=<n>]
##
## The scenario drives the same public API a player uses; it never touches a
## campaign field itself.

const WorldScene := preload("res://wotr_remaster/world/wotr_world.tscn")
const CorePort := preload("res://wotr_remaster/core/wotr_core_native_port.gd")

const MEASURE_FRAMES := 60
const GONDOR_ARMY := "army_gondor"
const MORDOR_ARMY := "army_mordor"
const CROSSING := "crossing"

var _failures: Array = []
var _steps: Array = []
var _world = null


func _initialize() -> void:
	_run()


func _run() -> void:
	var args := _user_args()
	var pack_path := str(args.get("pack", ""))
	var out_dir := str(args.get("out", "/tmp/wotr-k4"))
	var seed_value := int(str(args.get("seed", "20261121")))
	DirAccess.make_dir_recursive_absolute(out_dir)

	var report := {
		"pack": _repo_relative(pack_path),
		"seed": seed_value,
		"out_dir": out_dir,
		"steps": _steps,
		"hashes": {},
		"screenshots": [],
	}

	if pack_path.is_empty():
		_fail("no campaign pack was given (--pack=<file>)")
		_finish(report, out_dir)
		return

	# ---- the world -------------------------------------------------------
	_world = WorldScene.instantiate()
	root.add_child(_world)
	await process_frame

	var setup_result: Dictionary = _world.setup(pack_path, seed_value)
	_step("setup", setup_result)
	if not bool(setup_result.get("ok", false)):
		_fail("the world could not open the campaign: %s" % str(setup_result.get("error_message", "")))
		_finish(report, out_dir)
		return
	report["battle_port"] = {
		"name": str(setup_result.get("battle_port", "")),
		"native": bool(setup_result.get("battle_port_is_native", false)),
	}
	report["pack_hash"] = str(setup_result.get("pack_hash", ""))
	var hash_initial := str(setup_result.get("state_hash", ""))
	report["hashes"]["initial"] = hash_initial
	await process_frame
	await _capture(report, out_dir, "01-initial.png")

	# ---- the tiny map is what the pack says ------------------------------
	var snapshot: Dictionary = _world.snapshot
	var regions: Array = snapshot.get("regions", [])
	var armies: Array = snapshot.get("armies", [])
	_check(regions.size() == 3, "the tiny map has three regions (got %d)" % regions.size())
	_check(armies.size() == 2, "the tiny map has two armies (got %d)" % armies.size())

	# ---- select → move ---------------------------------------------------
	_check(bool(_world.select_army(GONDOR_ARMY)), "the local army %s can be selected" % GONDOR_ARMY)
	_check(bool(_world.preview_move(CROSSING)), "the move to %s can be previewed" % CROSSING)
	var move_one: Dictionary = _world.submit_move()
	_step("move gondor -> crossing", move_one)
	_check(bool(move_one.get("ok", false)), "the first move is accepted by the core")
	report["hashes"]["after_first_move"] = str(_world.core.state_hash())

	# a move the core must refuse: the state must not change
	var state_before_refused := str(_world.core.state_hash())
	_world.select_army(MORDOR_ARMY)
	_world.preview_move("west")
	var refused: Dictionary = _world.submit_move()
	_step("move mordor -> west (must be refused)", refused)
	_check(not bool(refused.get("ok", false)), "a move to a non-adjacent region is refused")
	_check(str(_world.core.state_hash()) == state_before_refused,
			"the refused move leaves the state hash unchanged")

	# ---- the conflict ----------------------------------------------------
	_check(bool(_world.select_army(MORDOR_ARMY)), "the second army %s can be selected" % MORDOR_ARMY)
	_check(bool(_world.preview_move(CROSSING)), "the second move to %s can be previewed" % CROSSING)
	var move_two: Dictionary = _world.submit_move()
	_step("move mordor -> crossing", move_two)
	_check(bool(move_two.get("ok", false)), "the second move is accepted by the core")
	var conflict_hash := str(_world.core.state_hash())
	report["hashes"]["after_conflict"] = conflict_hash
	var pending = _world.snapshot.get("pendingBattle", null)
	_check(pending != null, "the core reports a pending battle")
	var request_checksum := ""
	if pending != null:
		_check(str(pending.get("regionId", "")) == CROSSING, "the pending battle is in %s" % CROSSING)
		_check((pending.get("participants", []) as Array).size() == 2, "the pending battle has two participants")
		request_checksum = str(pending.get("requestChecksum", ""))
		_check(not request_checksum.is_empty(), "the pending battle carries its request checksum")
	await process_frame
	await _capture(report, out_dir, "02-conflict.png")

	# ---- battle launch → return ------------------------------------------
	var battle_result: Dictionary = _world.launch_battle()
	_step("battle launch", battle_result)
	report["battle"] = {
		"statistics": _world.battle_statistics(),
		"tactical_statistics": str(battle_result.get("tacticalStatistics", "")),
		"victor": str(battle_result.get("victor", "")),
		"completion_reason": int(battle_result.get("completionReason", 0)),
		"request_checksum": request_checksum,
		"result_checksum": str(battle_result.get("resultChecksum", "")),
	}
	_check(bool(battle_result.get("ok", false)), "the battle port returned a result")
	var applied: Dictionary = _world.apply_battle_result(battle_result)
	_step("battle result applied", applied)
	_check(bool(applied.get("ok", false)), "the core applied the battle result")
	var after_battle_hash := str(_world.core.state_hash())
	report["hashes"]["after_battle"] = after_battle_hash
	_check(after_battle_hash != conflict_hash, "the battle changed the campaign state")
	var crossing: Dictionary = {}
	for region in _world.snapshot.get("regions", []):
		if str(region.get("id", "")) == CROSSING:
			crossing = region
	_check(crossing.get("owner", null) != null, "the battle gave %s an owner" % CROSSING)
	var survivor_total := 0
	for army in _world.snapshot.get("armies", []):
		for member in army.get("members", []):
			survivor_total += int(member.get("quantity", 0))
	report["survivors_total"] = survivor_total
	_check(survivor_total < 24, "the battle left fewer units than the 24 it started with (got %d)" % survivor_total)
	await process_frame
	await _capture(report, out_dir, "03-after-battle.png")

	# a second apply must be refused and must not change the state
	var second_apply: Dictionary = _world.apply_battle_result(battle_result)
	_step("second apply (must be refused)", second_apply)
	_check(not bool(second_apply.get("ok", false)), "a second apply of the same result is refused")
	_check(str(_world.core.state_hash()) == after_battle_hash, "the refused apply leaves the state hash unchanged")

	# ---- save / load into a fresh instance --------------------------------
	var envelope: String = _world.save_campaign()
	_check(not envelope.is_empty(), "the campaign saves into an envelope")
	report["save"] = {
		"bytes": envelope.length(),
		"sha256": _sha256(envelope),
	}
	var fresh := CorePort.new()
	if not fresh.available:
		_fail("a fresh core session could not be created: %s" % fresh.unavailable_reason)
	else:
		var pack_file := FileAccess.open(pack_path, FileAccess.READ)
		var pack_text := pack_file.get_as_text() if pack_file != null else ""
		if pack_file != null:
			pack_file.close()
		var fresh_loaded: Dictionary = fresh.load_pack(pack_text)
		_check(bool(fresh_loaded.get("ok", false)), "a fresh session loads the pack")
		var fresh_save: Dictionary = fresh.load_save(envelope)
		_step("fresh session loads the save", fresh_save)
		_check(bool(fresh_save.get("ok", false)), "a fresh session loads the saved campaign")
		var fresh_hash := fresh.state_hash()
		report["save"]["fresh_state_hash"] = fresh_hash
		report["save"]["state_hash"] = after_battle_hash
		_check(fresh_hash == after_battle_hash, "the reloaded campaign has the same state hash")
		report["save"]["bytes_equal"] = fresh.state_snapshot() == _world.core.state_snapshot()
		_check(bool(report["save"]["bytes_equal"]), "the reloaded campaign has the same canonical state bytes")

	# ---- the measured facts ------------------------------------------------
	var frames := await _measure_frames(MEASURE_FRAMES)
	report["frames"] = frames
	report["memory"] = {
		"peak_rss_kb": _proc_status_kb("VmHWM:"),
		"rss_kb": _proc_status_kb("VmRSS:"),
	}
	_finish(report, out_dir)


func _step(name: String, result: Dictionary) -> void:
	var entry := {
		"name": name,
		"ok": bool(result.get("ok", false)),
		"error_code": str(result.get("error_code", "")),
		"error_message": str(result.get("error_message", "")),
	}
	_steps.append(entry)
	if bool(entry["ok"]):
		print("WOTR-K4 step ok: ", name)
	else:
		print("WOTR-K4 step failed: ", name, " (", entry["error_code"], ": ", entry["error_message"], ")")


func _check(condition: bool, message: String) -> void:
	if condition:
		print("WOTR-K4 check ok: ", message)
	else:
		_failures.append(message)
		print("WOTR-K4 CHECK FAILED: ", message)


func _fail(message: String) -> void:
	_failures.append(message)
	print("WOTR-K4 FAILED: ", message)


func _capture(report: Dictionary, out_dir: String, file_name: String) -> void:
	await RenderingServer.frame_post_draw
	var image := root.get_texture().get_image()
	if image == null:
		_fail("the frame could not be read for " + file_name)
		return
	var path := out_dir.path_join(file_name)
	var error := image.save_png(path)
	if error != OK:
		_fail("the screenshot %s could not be written (%d)" % [file_name, error])
		return
	report["screenshots"].append({
		"file": file_name,
		"path": path,
		"width": image.get_width(),
		"height": image.get_height(),
	})
	print("WOTR-K4 screenshot: ", path, " (", image.get_width(), "x", image.get_height(), ")")


func _measure_frames(count: int) -> Dictionary:
	# The first frames after a state change are warm-up (shaders, textures);
	# they are not the frame time of the map.
	for index in range(5):
		await process_frame
	var samples: Array[float] = []
	var previous := Time.get_ticks_usec()
	for index in range(count):
		await process_frame
		var now := Time.get_ticks_usec()
		samples.append(float(now - previous) / 1000.0)
		previous = now
	var total := 0.0
	var smallest := INF
	var largest := 0.0
	var sorted := samples.duplicate()
	sorted.sort()
	var median := 0.0
	if not sorted.is_empty():
		median = sorted[sorted.size() / 2]
	for sample in samples:
		total += sample
		smallest = min(smallest, sample)
		largest = max(largest, sample)
	return {
		"count": samples.size(),
		"min_ms": smallest,
		"median_ms": median,
		"avg_ms": total / float(max(samples.size(), 1)),
		"max_ms": largest,
		"total_ms": total,
	}


## The peak (VmHWM) and the current (VmRSS) resident set of this process, in
## kibibytes.
##
## /proc files have no size, so Godot's FileAccess cannot read them; the
## values are copied into a scratch file by the shell first. The number is the
## first whitespace-separated token after the field name ("VmHWM:\t 12345 kB").
func _proc_status_kb(field: String) -> int:
	var scratch := "user://wotr-k4-memory.txt"
	var scratch_path := ProjectSettings.globalize_path(scratch)
	var pid := OS.get_process_id()
	OS.execute("/bin/sh", ["-c", "grep -E '^(VmHWM|VmRSS):' /proc/%d/status > '%s' 2>/dev/null" % [pid, scratch_path]])
	var file := FileAccess.open(scratch, FileAccess.READ)
	if file == null:
		return -1
	var text := file.get_as_text()
	file.close()
	DirAccess.remove_absolute(scratch_path)
	for line in text.split("\n"):
		if not line.begins_with(field):
			continue
		var rest := line.substr(field.length()).strip_edges()
		var number := ""
		for character in rest:
			if character >= "0" and character <= "9":
				number += character
			elif not number.is_empty():
				break
		if number.is_empty():
			return -1
		return int(number)
	return -1


func _sha256(text: String) -> String:
	var context := HashingContext.new()
	context.start(HashingContext.HASH_SHA256)
	context.update(text.to_utf8_buffer())
	return context.finish().hex_encode()


## The path of a repository file as the repository spells it, so that the
## report of a run carries no machine-specific directory.
func _repo_relative(path: String) -> String:
	var root := ProjectSettings.globalize_path("res://").path_join("..").simplify_path()
	if path.begins_with(root + "/"):
		return path.substr(root.length() + 1)
	return path


func _user_args() -> Dictionary:
	var args := {}
	for raw in OS.get_cmdline_user_args():
		var text := str(raw)
		if not text.begins_with("--"):
			continue
		text = text.substr(2)
		var split := text.find("=")
		if split < 0:
			args[text] = "true"
		else:
			args[text.substr(0, split)] = text.substr(split + 1)
	return args


func _finish(report: Dictionary, out_dir: String) -> void:
	report["failures"] = _failures
	report["result"] = "pass" if _failures.is_empty() else "fail"
	var text := JSON.stringify(report, "  ")
	var path := out_dir.path_join("report.json")
	var file := FileAccess.open(path, FileAccess.WRITE)
	if file != null:
		file.store_string(text)
		file.close()
	print("WOTR-K4 REPORT ", text)
	if _failures.is_empty():
		print("WOTR-K4 PASS")
		quit(0)
	else:
		print("WOTR-K4 FAIL")
		quit(1)
