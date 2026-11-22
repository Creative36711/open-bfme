extends RefCounted
## WOTR-1.1-K4: the Godot side of the WotrRemaster strategic core.
##
## The class this port talks to is registered by the extension in
## godot/wotr_remaster/core/native/ (build it with
## godot/wotr_remaster/core/native/build_core_extension.sh). It holds the
## canonical campaign state in C++: this port only forwards command intents and
## reads the snapshots the core returns.

const NATIVE_CLASS := "WotrRemasterCoreNative"

var _core: RefCounted = null
var available := false
var unavailable_reason := ""


func _init() -> void:
	if not ClassDB.class_exists(NATIVE_CLASS):
		unavailable_reason = ("the native WotrRemaster core extension is not loaded; build it with "
			+ "godot/wotr_remaster/core/native/build_core_extension.sh")
		return
	_core = ClassDB.instantiate(NATIVE_CLASS)
	if _core == null:
		unavailable_reason = "the native WotrRemaster core class could not be instantiated"
		return
	available = true


func _call(method: String, args: Array) -> Dictionary:
	if not available:
		return {"ok": false, "error_code": "CoreUnavailable", "error_message": unavailable_reason}
	var result: Variant = _core.callv(method, args)
	if result is Dictionary:
		return result
	return {"ok": false, "error_code": "CoreError", "error_message": "the core returned no answer"}


func load_pack(pack_json: String) -> Dictionary:
	return _call("load_pack", [pack_json])


func initial_state(seed_value: int) -> Dictionary:
	return _call("initial_state", [seed_value])


func resolve_turn(commands: Array) -> Dictionary:
	return _call("resolve_turn", [JSON.stringify(commands)])


func apply_battle_result(result: Dictionary) -> Dictionary:
	return _call("apply_battle_result", [JSON.stringify(result)])


func state_snapshot() -> Dictionary:
	if not available:
		return {}
	var text: String = _core.call("state_snapshot")
	var parsed: Variant = JSON.parse_string(text)
	return parsed if parsed is Dictionary else {}


func state_hash() -> String:
	if not available:
		return ""
	return _core.call("state_hash")


func pack_hash() -> String:
	if not available:
		return ""
	return _core.call("pack_hash")


func save_envelope() -> String:
	if not available:
		return ""
	return _core.call("save_envelope")


func load_save(envelope_text: String) -> Dictionary:
	return _call("load_save", [envelope_text])


func battle_request() -> Dictionary:
	if not available:
		return {}
	var text: String = _core.call("battle_request_json")
	if text.is_empty():
		return {}
	var parsed: Variant = JSON.parse_string(text)
	return parsed if parsed is Dictionary else {}
