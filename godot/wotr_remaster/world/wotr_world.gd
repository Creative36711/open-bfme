extends Node3D
## WOTR-1.1-K4: the minimal 2.5D strategic map of the WotrRemaster.
##
## The world is presentation and input only. Every player action becomes a
## command intent for the WotrRemaster strategic core; the world then draws
## the state the core returned. It never writes a campaign field, never
## computes a checksum and never decides a battle.
##
## The tiny map: three regions (west, crossing, east), two factions, one army
## each. The geometry is presentation (mission §12 leaves it free) and the
## palette is in wotr_palette.gd.

const CorePort := preload("res://wotr_remaster/core/wotr_core_native_port.gd")
const BattleFactory := preload("res://wotr_remaster/battle/wotr_battle_port_factory.gd")
const CellView := preload("res://wotr_remaster/world/wotr_cell_view.gd")
const ArmyView := preload("res://wotr_remaster/world/wotr_army_view.gd")
const RouteView := preload("res://wotr_remaster/world/wotr_route_view.gd")
const Palette := preload("res://wotr_remaster/world/wotr_palette.gd")

## Where the three regions of the tiny pack sit on the board.
const REGION_LAYOUT := {
	"west": Vector3(-5.2, 0.0, 0.0),
	"crossing": Vector3(0.0, 0.0, 0.0),
	"east": Vector3(5.2, 0.0, 0.0),
}
const REGION_RADIUS := 1.9
const FALLBACK_RING := 5.2

var core = null
var battle = null
var battle_port_is_native := false
var battle_port_reason := ""
var local_faction := ""

var snapshot: Dictionary = {}
var selected_army := ""
var preview_region := ""
var pending_request: Dictionary = {}
var last_result: Dictionary = {}
var last_battle_result: Dictionary = {}
var last_save := ""

var _cells: Dictionary = {}
var _armies: Dictionary = {}
var _routes: Array = []
var _sequence := 0
var _hud = null


func _ready() -> void:
	_hud = get_node_or_null("Hud/Root") as Control
	if _hud != null and _hud.has_method("update"):
		_hud.world = self
		_hud.call("update", status_report())


## Opens a campaign pack and builds its first state.
func setup(pack_path: String, seed_value: int) -> Dictionary:
	core = CorePort.new()
	if not core.available:
		return {"ok": false, "error_code": "CoreUnavailable", "error_message": core.unavailable_reason}
	var file := FileAccess.open(pack_path, FileAccess.READ)
	if file == null:
		return {"ok": false, "error_code": "PackMissing",
				"error_message": "the campaign pack cannot be read: " + pack_path}
	var pack_text := file.get_as_text()
	file.close()
	var loaded: Dictionary = core.load_pack(pack_text)
	if not bool(loaded.get("ok", false)):
		return loaded
	var started: Dictionary = core.initial_state(seed_value)
	if not bool(started.get("ok", false)):
		return started
	var chosen: Dictionary = BattleFactory.create()
	battle = chosen["port"]
	battle_port_is_native = bool(chosen["native"])
	battle_port_reason = str(chosen["reason"])
	local_faction = _first_faction()
	refresh()
	return {
		"ok": true,
		"state_hash": core.state_hash(),
		"pack_hash": core.pack_hash(),
		"battle_port": battle.port_name(),
		"battle_port_is_native": battle_port_is_native,
	}


## Reads the state the core holds and redraws the map.
func refresh() -> void:
	if core == null:
		return
	snapshot = core.state_snapshot()
	_sync_cells()
	_sync_armies()
	_sync_routes()
	_update_hud()


## Selects one of the player's armies (view state, not campaign state).
func select_army(army_id: String) -> bool:
	var army := _army_by_id(army_id)
	if army.is_empty():
		_update_hud()
		return false
	selected_army = army_id
	preview_region = ""
	_update_hud()
	return true


## Points the planned move at a region (view state).
func preview_move(region_id: String) -> bool:
	if selected_army.is_empty():
		_update_hud()
		return false
	var army := _army_by_id(selected_army)
	if army.is_empty():
		_update_hud()
		return false
	preview_region = region_id
	_update_hud()
	return true


## Submits the planned move to the core as a command intent.
func submit_move() -> Dictionary:
	if selected_army.is_empty():
		return _fail("NoSelection", "no army is selected")
	if preview_region.is_empty():
		return _fail("NoTarget", "no target region is chosen")
	var army := _army_by_id(selected_army)
	if army.is_empty():
		return _fail("UnknownArmy", "the selected army is not in the campaign")
	var from_region := str(army.get("regionId", ""))
	_sequence += 1
	var intent := {
		"playerId": str(army.get("factionId", "")),
		"clientSequence": _sequence,
		"commandId": "cmd-%d" % _sequence,
		"armyId": selected_army,
		"targetRegionId": preview_region,
	}
	var answer: Dictionary = core.resolve_turn([intent])
	last_result = answer
	if bool(answer.get("ok", false)):
		pending_request = answer.get("battle_request", {})
		var color := Palette.faction_color(str(army.get("factionId", "")), _faction_order())
		_routes.append({
			"from": _region_position(from_region),
			"to": _region_position(preview_region),
			"color": color,
		})
	refresh()
	return answer


## Runs the pending battle through the battle port and returns its result.
func launch_battle() -> Dictionary:
	if pending_request.is_empty():
		return _fail("NoBattle", "there is no pending battle")
	var result: Dictionary = battle.run(pending_request)
	last_battle_result = result
	return result


## What the last battle reported about itself (empty when it reports nothing).
func battle_statistics() -> Dictionary:
	if battle == null or not battle.has_method("last_statistics"):
		return {}
	return battle.last_statistics()


## Hands the battle result to the core (once).
func apply_battle_result(result: Dictionary) -> Dictionary:
	var answer: Dictionary = core.apply_battle_result(result)
	last_result = answer
	if bool(answer.get("ok", false)):
		pending_request = {}
	refresh()
	return answer


## The canonical save envelope of the campaign.
func save_campaign() -> String:
	last_save = core.save_envelope()
	return last_save


## Loads a save envelope into this session.
func load_campaign(envelope_text: String) -> Dictionary:
	var answer: Dictionary = core.load_save(envelope_text)
	last_result = answer
	refresh()
	return answer


## Reloads the last save this world wrote.
func reload_last_save() -> Dictionary:
	if last_save.is_empty():
		return _fail("NoSave", "this world has not saved yet")
	return load_campaign(last_save)


## The data the HUD shows.
func status_report() -> Dictionary:
	var phase := "planning"
	var turn := 0
	var pack_id := ""
	var profile := ""
	var tactical_map := ""
	var hash := ""
	if not snapshot.is_empty():
		phase = _phase_name(int(snapshot.get("phase", 0)))
		turn = int(snapshot.get("turn", 0))
		pack_id = str(snapshot.get("packId", ""))
		profile = str(snapshot.get("tacticalProfile", ""))
		tactical_map = str(snapshot.get("tacticalMap", ""))
		hash = core.state_hash() if core != null else ""
	var regions: Array = []
	for region in snapshot.get("regions", []):
		var owner = region.get("owner", null)
		regions.append("%s=%s" % [str(region.get("id", "")), "-" if owner == null else str(owner)])
	var selection := "no army selected"
	if not selected_army.is_empty():
		selection = "selected %s" % selected_army
		if not preview_region.is_empty():
			selection += " -> %s" % preview_region
	var battle_line := "no pending battle"
	var pending = snapshot.get("pendingBattle", null)
	if pending != null:
		battle_line = "pending %s in %s (%d participants)" % [
			str(pending.get("id", "")),
			str(pending.get("regionId", "")),
			(pending.get("participants", []) as Array).size(),
		]
	var result_line := ""
	if not last_result.is_empty():
		if bool(last_result.get("ok", false)):
			result_line = "core ok"
		else:
			result_line = "core %s: %s" % [
				str(last_result.get("error_code", "")),
				str(last_result.get("error_message", "")),
			]
	return {
		"title": "%s · %s · %s" % [pack_id, profile, tactical_map],
		"status": "turn %d · %s · state %s" % [turn, phase, hash.substr(0, 12) if hash.length() > 12 else hash],
		"selection": selection,
		"regions": "regions: " + " ".join(regions),
		"battle": battle_line,
		"result": result_line,
		"can_move": not selected_army.is_empty() and not preview_region.is_empty(),
	}


## The region a click landed on ("" when the click missed the map).
func region_at_screen(screen_position: Vector2) -> String:
	var camera := get_node_or_null("Camera3D") as Camera3D
	if camera == null:
		return ""
	var origin := camera.project_ray_origin(screen_position)
	var direction := camera.project_ray_normal(screen_position)
	if absf(direction.y) < 0.0001:
		return ""
	var distance := -origin.y / direction.y
	if distance < 0.0:
		return ""
	var point := origin + direction * distance
	var best := ""
	var best_distance := INF
	for region_id in _cells:
		var cell: Node3D = _cells[region_id]
		var cell_distance := Vector2(point.x - cell.position.x, point.z - cell.position.z).length()
		if cell_distance <= REGION_RADIUS and cell_distance < best_distance:
			best_distance = cell_distance
			best = region_id
	return best


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		var mouse := event as InputEventMouseButton
		if mouse.pressed and mouse.button_index == MOUSE_BUTTON_LEFT:
			_click_at(mouse.position)


func _click_at(screen_position: Vector2) -> void:
	var region_id := region_at_screen(screen_position)
	if region_id.is_empty():
		return
	var army := _army_at_region(region_id)
	if not army.is_empty() and str(army.get("factionId", "")) == local_faction:
		select_army(str(army.get("id", "")))
		return
	if not selected_army.is_empty():
		preview_move(region_id)


func _sync_cells() -> void:
	for region in snapshot.get("regions", []):
		var region_id := str(region.get("id", ""))
		if region_id.is_empty():
			continue
		var cell: Node3D = null
		if _cells.has(region_id):
			cell = _cells[region_id]
		else:
			cell = CellView.new()
			cell.setup(region_id, _region_position(region_id))
			add_child(cell)
			_cells[region_id] = cell
		var owner = region.get("owner", null)
		var color := Palette.CELL_TOP
		if owner != null:
			color = Palette.faction_color(str(owner), _faction_order())
		cell.call("set_owner_color", color)
		cell.call("set_selected", region_id == preview_region)
		var pending = snapshot.get("pendingBattle", null)
		var in_conflict := pending != null and str(pending.get("regionId", "")) == region_id
		cell.call("set_conflict", in_conflict)


func _sync_armies() -> void:
	var per_region := {}
	for army in snapshot.get("armies", []):
		var army_id := str(army.get("id", ""))
		if army_id.is_empty():
			continue
		var region_id := str(army.get("regionId", ""))
		var slot: int = per_region.get(region_id, 0)
		per_region[region_id] = slot + 1
		var view: Node3D = null
		if _armies.has(army_id):
			view = _armies[army_id]
			view.position = _army_position(region_id, slot)
		else:
			view = ArmyView.new()
			view.setup(army_id, _army_position(region_id, slot), slot)
			add_child(view)
			_armies[army_id] = view
		var strength := 0
		var kinds: Array = []
		for member in army.get("members", []):
			strength += int(member.get("quantity", 0))
			kinds.append(str(member.get("kind", "")))
		var caption := "%s x%d" % [army_id, strength]
		if not kinds.is_empty():
			caption += "\n" + ", ".join(kinds)
		view.call("set_army", Palette.faction_color(str(army.get("factionId", "")), _faction_order()), strength, caption)
		view.call("set_selected", army_id == selected_army)


func _sync_routes() -> void:
	var route_view := get_node_or_null("Routes") as Node3D
	if route_view == null:
		return
	var drawn: Array = []
	for route in _routes:
		drawn.append(route)
	if not preview_region.is_empty() and not selected_army.is_empty():
		var army := _army_by_id(selected_army)
		if not army.is_empty():
			drawn.append({
				"from": _region_position(str(army.get("regionId", ""))),
				"to": _region_position(preview_region),
				"color": Palette.faction_color(str(army.get("factionId", "")), _faction_order()),
			})
	route_view.call("set_routes", drawn)


func _army_by_id(army_id: String) -> Dictionary:
	for army in snapshot.get("armies", []):
		if str(army.get("id", "")) == army_id:
			return army
	return {}


func _army_at_region(region_id: String) -> Dictionary:
	for army in snapshot.get("armies", []):
		if str(army.get("regionId", "")) == region_id:
			return army
	return {}


func _region_order() -> Array:
	var ids: Array = []
	for region in snapshot.get("regions", []):
		ids.append(str(region.get("id", "")))
	ids.sort()
	return ids


func _faction_order() -> Array:
	var ids: Array = []
	for faction in snapshot.get("factions", []):
		ids.append(str(faction.get("id", "")))
	ids.sort()
	return ids


func _first_faction() -> String:
	var order := _faction_order()
	return str(order[0]) if not order.is_empty() else ""


func _region_position(region_id: String) -> Vector3:
	if REGION_LAYOUT.has(region_id):
		return REGION_LAYOUT[region_id]
	var order := _region_order()
	var index: int = order.find(region_id)
	if index < 0:
		return Vector3.ZERO
	var angle := TAU * float(index) / float(max(order.size(), 1))
	return Vector3(cos(angle) * FALLBACK_RING, 0.0, sin(angle) * FALLBACK_RING)


func _army_position(region_id: String, slot: int) -> Vector3:
	var base := _region_position(region_id)
	var offset := Vector3(-0.7 + 1.4 * float(slot), 0.12, 0.5)
	return base + offset


func _phase_name(phase: int) -> String:
	match phase:
		0:
			return "planning"
		1:
			return "resolving"
		2:
			return "battle pending"
	return "unknown"


func _fail(code: String, message: String) -> Dictionary:
	return {"ok": false, "error_code": code, "error_message": message}


func _update_hud() -> void:
	if _hud != null and _hud.has_method("update"):
		_hud.call("update", status_report())
