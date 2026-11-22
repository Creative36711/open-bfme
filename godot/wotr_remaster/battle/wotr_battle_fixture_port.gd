extends RefCounted
## WOTR-1.1-K4: the deterministic fixture battle.
##
## This is NOT a native tactical capture. The native bridge (this card) needs
## the extension and the mounted retail trees; without them the world's battle
## step would otherwise have no tactical side at all. This port resolves the
## BattleRequestV1 with a fixed rule that reads nothing but the request:
##
##   * a side's strength is the sum of its members' quantities;
##   * the stronger side wins, the lower side index wins a tie;
##   * the winner keeps ceil(2/3) of every member, the loser floor(1/3);
##   * every survivor gains one experience point.
##
## The result therefore carries no tactical statistics and no capture frame:
## both are explicitly empty, not silently dropped.
##
## It is a stand-in, and the report of every run that used it says so.

const Port := preload("res://wotr_remaster/battle/wotr_battle_port.gd")

var _base: RefCounted = null


func _init() -> void:
	_base = Port.new()


func is_available() -> bool:
	return true


func unavailable_reason() -> String:
	return ""


func port_name() -> String:
	return "fixture (deterministic, not a native capture)"


func last_statistics() -> Dictionary:
	return {}


func run(request: Dictionary) -> Dictionary:
	var request_id: String = str(request.get("id", ""))
	var request_checksum: String = str(request.get("requestChecksum", ""))
	if request_id.is_empty() or request_checksum.is_empty():
		return {"ok": false, "error_code": "InvalidRequest", "error_message": "the battle request has no id or checksum"}

	var participants: Array = request.get("participants", [])
	if participants.is_empty():
		return {"ok": false, "error_code": "InvalidRequest", "error_message": "the battle request has no participants"}

	var totals := {}
	var factions := {}
	for participant in participants:
		var army_id: String = str(participant.get("id", ""))
		var side: int = int(participant.get("side", 0))
		var faction_id: String = str(participant.get("factionId", ""))
		var strength := 0
		for member in participant.get("members", []):
			strength += int(member.get("quantity", 0))
		totals[side] = int(totals.get(side, 0)) + strength
		factions[side] = faction_id
		if army_id.is_empty() or faction_id.is_empty():
			return {"ok": false, "error_code": "InvalidRequest", "error_message": "a participant has no army or faction id"}

	var sides := totals.keys()
	sides.sort()
	var winner_side: int = int(sides[0])
	var loser_side: int = int(sides[0])
	for side in sides:
		if int(totals[side]) > int(totals[winner_side]):
			winner_side = int(side)
	if sides.size() > 1:
		for side in sides:
			if int(side) != winner_side:
				loser_side = int(side)
				break

	var survivors: Array = []
	for participant in participants:
		var side: int = int(participant.get("side", 0))
		var members: Array = []
		for member in participant.get("members", []):
			var quantity: int = int(member.get("quantity", 0))
			var survivors_count := quantity / 3
			if side == winner_side:
				survivors_count = (quantity * 2 + 2) / 3
			if quantity > 0 and survivors_count < 1:
				survivors_count = 1
			members.append({
				"id": str(member.get("id", "")),
				"quantity": survivors_count,
				"experience": int(member.get("experience", 0)) + 1,
				"persistentUpgrades": [],
			})
		survivors.append({"id": str(participant.get("id", "")), "members": members})

	return {
		"ok": true,
		"contractVersion": 1,
		"id": request_id,
		"requestChecksum": request_checksum,
		"completionReason": 1,
		"victor": str(factions.get(winner_side, "")),
		"survivors": survivors,
		"heroes": [],
		"persistentSciences": [],
		"reinforcements": {"called": [], "uncalled": []},
		"destroyedObjectives": [],
		"tacticalStatistics": "",
		"captureFrame": null,
	}
