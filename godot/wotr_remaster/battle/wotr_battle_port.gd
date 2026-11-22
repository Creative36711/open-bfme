extends RefCounted
## WOTR-1.1-K4: the strategic -> tactical boundary of the Remaster world.
##
## The world hands the core's BattleRequestV1 to a battle port and gets a
## BattleResultV1 back; it never decides a battle itself. The native bridge
## (this card) is the production implementation; when it cannot run - no
## extension, no retail trees - the fixture port answers, and it says so.

## Whether this port can run a battle in this build.
func is_available() -> bool:
	return false


## Why the port cannot run a battle (empty when it can).
func unavailable_reason() -> String:
	return "no battle port"


## A short name of the implementation, for the report.
func port_name() -> String:
	return "none"


## What the last battle reported about itself (empty when it reports nothing).
func last_statistics() -> Dictionary:
	return {}


## Runs the battle described by the request.
##
## The request is the core's BattleRequestV1 (read-only). The answer is either
## a BattleResultV1 Dictionary or an error Dictionary with ok = false.
func run(_request: Dictionary) -> Dictionary:
	return {"ok": false, "error_code": "NotImplemented", "error_message": "no battle port"}
