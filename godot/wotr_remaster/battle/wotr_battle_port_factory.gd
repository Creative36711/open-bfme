extends RefCounted
## WOTR-1.1-K4: picks the battle port of this build.
##
## The native bridge wins when it is registered in the running build and the
## retail trees are mounted. Otherwise the deterministic fixture answers, and
## the run records which port it used, so no report can claim a native capture
## that did not happen.

const BridgePort := preload("res://wotr_remaster/battle/wotr_battle_bridge_port.gd")
const FixturePort := preload("res://wotr_remaster/battle/wotr_battle_fixture_port.gd")


## The port to use, and why it was chosen.
static func create() -> Dictionary:
	var bridge: RefCounted = BridgePort.new()
	if bridge.is_available():
		return {"port": bridge, "native": true, "reason": "the native tactical bridge is registered"}
	return {
		"port": FixturePort.new(),
		"native": false,
		"reason": bridge.unavailable_reason(),
	}
