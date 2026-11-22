extends RefCounted
## WOTR-1.1-K4: the binding to the native tactical bridge.
##
## The bridge is a GDExtension class of this build that runs OpenBFME's own
## War of the Ring battle for the core's BattleRequestV1 over the mounted
## retail archives and returns the native BattleResultV1. The Godot world
## only ever sees the two JSON documents, so the bridge stays the single
## owner of the tactical boundary.
##
## Contract (godot/wotr_remaster/core/native/wotr_battle_bridge_native.h):
##   class WotrBattleBridge : RefCounted
##       func run_battle(request_json: String, profile_json: String) -> Dictionary
##           # { ok, error_code, error_message, result: <BattleResultV1 JSON>,
##           #   statistics: { rounds, kills_side0, kills_side1, ... } }
##
## The tactical profile is product data: it names the retail stack template
## every catalog unit of the pack fights as, for the tactical profile the
## request asks for. The bridge reads the retail side, the profile names the
## product side; neither invents the other.

const Port := preload("res://wotr_remaster/battle/wotr_battle_port.gd")
const NATIVE_CLASS := "WotrBattleBridge"
const NATIVE_METHOD := "run_battle"
const PROFILE_DIR := "res://wotr_remaster/core/native/profiles/"

var _base: RefCounted = null
var _bridge: RefCounted = null
var available := false
var reason := ""
var _statistics: Dictionary = {}


func _init() -> void:
	_base = Port.new()
	if not ClassDB.class_exists(NATIVE_CLASS):
		reason = ("the native tactical bridge (%s) is not registered in this build; "
			+ "build godot/wotr_remaster/core/native/build_core_extension.sh first") % NATIVE_CLASS
		return
	_bridge = ClassDB.instantiate(NATIVE_CLASS)
	if _bridge == null:
		reason = "the native tactical bridge could not be instantiated"
		return
	if not _bridge.has_method(NATIVE_METHOD):
		reason = ("the native tactical bridge has no %s(request_json, profile_json) method"
			% NATIVE_METHOD)
		return
	# The bridge needs the validated retail trees: without them it refuses
	# the request instead of faking a fight.
	var status: Dictionary = _bridge.call("retail_status")
	if not bool(status.get("mounted", false)) and (
			str(status.get("rotwk_install", "")).is_empty()
			or str(status.get("bfme2_install", "")).is_empty()):
		reason = ("the retail trees are not mounted: ROTWK_INSTALL and BFME2_INSTALL must "
			+ "name the validated RotWK 2.01 and BFME2 1.06 folders")
		return
	available = true


func is_available() -> bool:
	return available


func unavailable_reason() -> String:
	return reason


func port_name() -> String:
	return "native bridge (%s, OpenBFME's own battle over the retail archives)" % NATIVE_CLASS


func last_statistics() -> Dictionary:
	return _statistics


func run(request: Dictionary) -> Dictionary:
	if not available:
		return {"ok": false, "error_code": "BridgeUnavailable", "error_message": reason}
	var profile_id: String = str(request.get("tacticalProfile", ""))
	var profile_path: String = "%s%s.json" % [PROFILE_DIR, profile_id]
	if not FileAccess.file_exists(profile_path):
		return {
			"ok": false,
			"error_code": "InvalidPack",
			"error_message": "the tactical profile '%s' is not in this build" % profile_id,
		}
	var profile_text: String = FileAccess.get_file_as_string(profile_path)
	var answer: Variant = _bridge.call(NATIVE_METHOD, JSON.stringify(request), profile_text)
	if not (answer is Dictionary):
		return {"ok": false, "error_code": "BridgeError", "error_message": "the native bridge returned no answer"}
	var bridge_answer: Dictionary = answer
	if not bool(bridge_answer.get("ok", false)):
		return {
			"ok": false,
			"error_code": str(bridge_answer.get("error_code", "BridgeError")),
			"error_message": str(bridge_answer.get("error_message", "")),
		}
	_statistics = bridge_answer.get("statistics", {})
	var result: Variant = bridge_answer.get("result", {})
	if not (result is Dictionary):
		return {"ok": false, "error_code": "BridgeError", "error_message": "the native bridge returned no result document"}
	var document: Dictionary = result
	document["ok"] = true
	return document
