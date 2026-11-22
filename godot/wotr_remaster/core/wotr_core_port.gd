extends RefCounted
## WOTR-1.1-K4: the command-only boundary between the Godot world and the
## WotrRemaster deterministic strategic core.
##
## The world never holds a campaign state it could change: it asks the port to
## resolve command intents and reads the state the core hands back. This
## script is the contract; godot/wotr_remaster/core/native/wotr_core_native.cpp
## is its only implementation (the canonical C++ core, bound to Godot).
##
## Every method returns a Dictionary with:
##   ok            bool    - the core accepted the request
##   error_code    String  - the core's named error code ("Ok" when it worked)
##   error_message String  - the core's message with context

## Loads and validates a Campaign Pack document (canonical JSON).
func load_pack(_pack_json: String) -> Dictionary:
	return {"ok": false, "error_code": "NotImplemented", "error_message": "no core port"}


## Builds the initial campaign state from the loaded pack and an explicit seed.
func initial_state(_seed_value: int) -> Dictionary:
	return {"ok": false, "error_code": "NotImplemented", "error_message": "no core port"}


## Resolves a WEGO batch of command intents.
##
## A command intent is a plain description of what the player wants:
##   {"playerId": String, "clientSequence": int, "commandId": String,
##    "armyId": String, "targetRegionId": String}
## The core builds the canonical envelope (campaign, turn, payload, checksum)
## and validates every command; nothing else is accepted.
##
## The answer carries "events" (the core's resolution events) and, when the
## resolution created a conflict, "battle_request" (the BattleRequestV1).
func resolve_turn(_commands: Array) -> Dictionary:
	return {"ok": false, "error_code": "NotImplemented", "error_message": "no core port"}


## Applies a battle result once. The core rejects a wrong battle id, a
## mismatched request checksum, a damaged checksum and a second apply.
func apply_battle_result(_result: Dictionary) -> Dictionary:
	return {"ok": false, "error_code": "NotImplemented", "error_message": "no core port"}


## The canonical state of the campaign, as a read-only Dictionary.
func state_snapshot() -> Dictionary:
	return {}


## The canonical state hash (SHA-256 over the canonical state bytes).
func state_hash() -> String:
	return ""


## The canonical content hash of the loaded pack.
func pack_hash() -> String:
	return ""


## The canonical save envelope (WotrSaveEnvelopeV1).
func save_envelope() -> String:
	return ""


## Loads a save envelope into this session, transactionally.
func load_save(_envelope_text: String) -> Dictionary:
	return {"ok": false, "error_code": "NotImplemented", "error_message": "no core port"}


## The pending BattleRequestV1, as a read-only Dictionary (empty when none).
func battle_request() -> Dictionary:
	return {}
