// OpenBFME. GPL-3.0.
// WOTR-1.1-K4 (Godot World Engineer): the native tactical bridge.
//
// The bridge runs OpenBFME's own War of the Ring battle for a BattleRequestV1
// and returns the native BattleResultV1. The implementation is in
// wotr_battle_bridge_native.cpp; this header only declares the Godot class.

#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

class WotrBattleBridge : public RefCounted {
	GDCLASS(WotrBattleBridge, RefCounted)

protected:
	static void _bind_methods();

public:
	/// Runs the native battle of a request over the mounted retail archives.
	/// { ok, error_code, error_message, result: <canonical JSON>, statistics: {...} }
	Dictionary run_battle(const String &p_request_json, const String &p_profile_json);

	/// What the retail mount of this process looks like (evidence).
	Dictionary retail_status();
};

} // namespace godot

// Registers the bridge class with Godot (called from the extension's init).
void register_wotr_battle_bridge();
