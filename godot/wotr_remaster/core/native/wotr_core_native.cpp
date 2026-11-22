// OpenBFME. GPL-3.0.
// WOTR-1.1-K4 (Godot World Engineer): the Godot binding of the WotrRemaster
// deterministic strategic core.
//
// The strategic core is NOT re-implemented here. The canonical C++ sources in
// engine/src/GameLogic/WotrRemaster (Strategic Core Engineer, card
// WOTR-1.1-K2) are compiled into this extension unchanged; this file only
// converts between Godot values and the core's typed contract.
//
// The boundary is deliberately one-way for state: GDScript may submit command
// intents, ask the core to resolve them, hand a battle result back and ask for
// a snapshot. It can never write a field of the campaign state, and it never
// computes a checksum: envelope, request and result checksums are produced by
// the core itself (envelopeChecksum / battleRequestChecksum /
// battleResultChecksum), so a Godot-side mistake cannot forge one.
//
// Every failure is returned as a named error code (the core's WotrErrc names)
// plus a message with context. Nothing is thrown across the boundary.

#include "Errors.h"
#include "Json.h"
#include "Pack.h"
#include "Save.h"
#include "State.h"
#include "wotr_battle_bridge_native.h"

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include <cstdint>
#include <exception>
#include <string>
#include <vector>

using namespace godot;
using namespace WotrRemaster;

namespace {

String to_gd(const std::string &p_text) {
	return String::utf8(p_text.c_str(), static_cast<int64_t>(p_text.size()));
}

std::string to_std(const String &p_text) {
	const CharString utf8 = p_text.utf8();
	return std::string(utf8.get_data(), static_cast<size_t>(utf8.length()));
}

Dictionary ok_result() {
	Dictionary out;
	out["ok"] = true;
	out["error_code"] = "Ok";
	out["error_message"] = "";
	return out;
}

Dictionary fail_result(WotrErrc p_code, const std::string &p_message) {
	Dictionary out;
	out["ok"] = false;
	out["error_code"] = to_gd(errcName(p_code));
	out["error_message"] = to_gd(p_message);
	return out;
}

uint64_t json_uint(const JsonValue &p_value) {
	if (p_value.type == JsonValue::Type::Int) {
		return static_cast<uint64_t>(p_value.intValue);
	}
	return p_value.uintValue;
}

Variant json_to_variant(const JsonValue &p_value) {
	switch (p_value.type) {
		case JsonValue::Type::Null:
			return Variant();
		case JsonValue::Type::Bool:
			return p_value.boolean;
		case JsonValue::Type::Int:
			return static_cast<int64_t>(p_value.intValue);
		case JsonValue::Type::UInt:
			return static_cast<int64_t>(p_value.uintValue);
		case JsonValue::Type::Double:
			return p_value.doubleValue;
		case JsonValue::Type::String:
			return to_gd(p_value.stringValue);
		case JsonValue::Type::Array: {
			Array out;
			for (const JsonValue &item : p_value.arrayValue) {
				out.append(json_to_variant(item));
			}
			return out;
		}
		case JsonValue::Type::Object: {
			Dictionary out;
			for (const auto &kv : p_value.objectValue) {
				out[to_gd(kv.first)] = json_to_variant(kv.second);
			}
			return out;
		}
	}
	return Variant();
}

JsonValue jstr(const std::string &p_value) {
	JsonValue out;
	out.type = JsonValue::Type::String;
	out.stringValue = p_value;
	return out;
}

JsonValue juint(uint64_t p_value) {
	JsonValue out;
	out.type = JsonValue::Type::UInt;
	out.uintValue = p_value;
	return out;
}

JsonValue jbool(bool p_value) {
	JsonValue out;
	out.type = JsonValue::Type::Bool;
	out.boolean = p_value;
	return out;
}

JsonValue jarray() {
	JsonValue out;
	out.type = JsonValue::Type::Array;
	return out;
}

JsonValue jobject() {
	JsonValue out;
	out.type = JsonValue::Type::Object;
	return out;
}

// BattleRequestV1 -> canonical JSON for the tactical side. The checksum stays
// the core's: this document only carries the §6 fields.
JsonValue request_to_json(const BattleRequestV1 &p_request) {
	JsonValue out = jobject();
	out.objectValue.emplace("contractVersion", juint(p_request.contractVersion));
	out.objectValue.emplace("id", jstr(p_request.id));
	out.objectValue.emplace("stateHash", jstr(p_request.stateHash));
	out.objectValue.emplace("packId", jstr(p_request.packId));
	out.objectValue.emplace("packHash", jstr(p_request.packHash));
	out.objectValue.emplace("tacticalProfile", jstr(p_request.tacticalProfile));
	out.objectValue.emplace("tacticalMap", jstr(p_request.tacticalMap));
	out.objectValue.emplace("tacticalSeed", juint(p_request.tacticalSeed));
	out.objectValue.emplace("regionId", jstr(p_request.regionId));
	out.objectValue.emplace("requestChecksum", jstr(p_request.requestChecksum));

	JsonValue participants = jarray();
	for (const TacticalArmy &army : p_request.participants) {
		JsonValue item = jobject();
		item.objectValue.emplace("id", jstr(army.id));
		item.objectValue.emplace("factionId", jstr(army.factionId));
		item.objectValue.emplace("side", juint(army.side));
		JsonValue members = jarray();
		for (const TacticalMember &member : army.members) {
			JsonValue mitem = jobject();
			mitem.objectValue.emplace("id", jstr(member.id));
			mitem.objectValue.emplace("templateId", jstr(member.templateId));
			mitem.objectValue.emplace("quantity", juint(member.quantity));
			mitem.objectValue.emplace("experience", juint(member.experience));
			JsonValue upgrades = jarray();
			for (const UpgradeId &upgrade : member.persistentUpgrades) {
				upgrades.arrayValue.push_back(jstr(upgrade));
			}
			mitem.objectValue.emplace("persistentUpgrades", std::move(upgrades));
			members.arrayValue.push_back(std::move(mitem));
		}
		item.objectValue.emplace("members", std::move(members));
		participants.arrayValue.push_back(std::move(item));
	}
	out.objectValue.emplace("participants", std::move(participants));

	JsonValue seats = jarray();
	for (const TacticalSeat &seat : p_request.seats) {
		JsonValue item = jobject();
		item.objectValue.emplace("playerId", jstr(seat.playerId));
		item.objectValue.emplace("human", jbool(seat.human));
		seats.arrayValue.push_back(std::move(item));
	}
	out.objectValue.emplace("seats", std::move(seats));

	out.objectValue.emplace("startingResources", juint(p_request.startingResources));

	JsonValue sciences = jarray();
	for (const std::string &science : p_request.persistentSciences) {
		sciences.arrayValue.push_back(jstr(science));
	}
	out.objectValue.emplace("persistentSciences", std::move(sciences));

	JsonValue reinforcements = jarray();
	for (const std::string &reinforcement : p_request.reinforcements) {
		reinforcements.arrayValue.push_back(jstr(reinforcement));
	}
	out.objectValue.emplace("reinforcements", std::move(reinforcements));

	JsonValue objectives = jarray();
	for (const RegionId &objective : p_request.strategicObjectives) {
		objectives.arrayValue.push_back(jstr(objective));
	}
	out.objectValue.emplace("strategicObjectives", std::move(objectives));

	JsonValue heroes = jarray();
	for (const MemberId &hero : p_request.heroes) {
		heroes.arrayValue.push_back(jstr(hero));
	}
	out.objectValue.emplace("heroes", std::move(heroes));
	return out;
}

// BattleResultV1 <- canonical JSON. Missing optional fields are represented
// as explicit absence, never silently dropped.
bool result_from_json(const JsonValue &p_root, BattleResultV1 &r_result, std::string &r_error) {
	if (!p_root.isObject()) {
		r_error = "battle result must be a JSON object";
		return false;
	}
	const JsonValue *id = p_root.get("id");
	const JsonValue *request_checksum = p_root.get("requestChecksum");
	if (id == nullptr || !id->isString() || request_checksum == nullptr || !request_checksum->isString()) {
		r_error = "battle result needs 'id' and 'requestChecksum'";
		return false;
	}
	r_result.id = id->stringValue;
	r_result.requestChecksum = request_checksum->stringValue;

	const JsonValue *version = p_root.get("contractVersion");
	r_result.contractVersion = (version != nullptr && version->isNumber()) ? static_cast<uint32_t>(json_uint(*version)) : 1;

	const JsonValue *reason = p_root.get("completionReason");
	if (reason == nullptr || !reason->isNumber()) {
		r_error = "battle result needs 'completionReason'";
		return false;
	}
	r_result.completionReason = static_cast<CompletionReason>(json_uint(*reason));

	const JsonValue *victor = p_root.get("victor");
	if (victor != nullptr && victor->isString()) {
		r_result.victor = victor->stringValue;
	}

	const JsonValue *survivors = p_root.get("survivors");
	if (survivors == nullptr || !survivors->isArray()) {
		r_error = "battle result needs a 'survivors' array";
		return false;
	}
	for (const JsonValue &army : survivors->arrayValue) {
		const JsonValue *army_id = army.get("id");
		if (army_id == nullptr || !army_id->isString()) {
			r_error = "a survivor army needs an 'id'";
			return false;
		}
		SurvivingArmy out_army;
		out_army.id = army_id->stringValue;
		const JsonValue *members = army.get("members");
		if (members != nullptr && members->isArray()) {
			for (const JsonValue &member : members->arrayValue) {
				const JsonValue *member_id = member.get("id");
				if (member_id == nullptr || !member_id->isString()) {
					r_error = "a surviving member needs an 'id'";
					return false;
				}
				SurvivingMember out_member;
				out_member.id = member_id->stringValue;
				const JsonValue *quantity = member.get("quantity");
				const JsonValue *experience = member.get("experience");
				out_member.quantity = (quantity != nullptr && quantity->isNumber()) ? static_cast<uint32_t>(json_uint(*quantity)) : 0;
				out_member.experience = (experience != nullptr && experience->isNumber()) ? static_cast<uint32_t>(json_uint(*experience)) : 0;
				const JsonValue *upgrades = member.get("persistentUpgrades");
				if (upgrades != nullptr && upgrades->isArray()) {
					for (const JsonValue &upgrade : upgrades->arrayValue) {
						if (upgrade.isString()) {
							out_member.persistentUpgrades.push_back(upgrade.stringValue);
						}
					}
				}
				out_army.members.push_back(std::move(out_member));
			}
		}
		r_result.survivors.push_back(std::move(out_army));
	}

	const JsonValue *heroes = p_root.get("heroes");
	if (heroes != nullptr && heroes->isArray()) {
		for (const JsonValue &hero : heroes->arrayValue) {
			const JsonValue *hero_id = hero.get("id");
			if (hero_id == nullptr || !hero_id->isString()) {
				r_error = "a hero record needs an 'id'";
				return false;
			}
			HeroRecord out_hero;
			out_hero.id = hero_id->stringValue;
			const JsonValue *survived = hero.get("survived");
			const JsonValue *revived = hero.get("revived");
			out_hero.survived = survived != nullptr && survived->type == JsonValue::Type::Bool && survived->boolean;
			out_hero.revived = revived != nullptr && revived->type == JsonValue::Type::Bool && revived->boolean;
			r_result.heroes.push_back(std::move(out_hero));
		}
	}

	const JsonValue *sciences = p_root.get("persistentSciences");
	if (sciences != nullptr && sciences->isArray()) {
		for (const JsonValue &science : sciences->arrayValue) {
			if (science.isString()) {
				r_result.persistentSciences.push_back(science.stringValue);
			}
		}
	}

	const JsonValue *reinforcements = p_root.get("reinforcements");
	if (reinforcements != nullptr && reinforcements->isObject()) {
		const JsonValue *called = reinforcements->get("called");
		const JsonValue *uncalled = reinforcements->get("uncalled");
		if (called != nullptr && called->isArray()) {
			for (const JsonValue &army : called->arrayValue) {
				if (army.isString()) {
					r_result.reinforcements.called.push_back(army.stringValue);
				}
			}
		}
		if (uncalled != nullptr && uncalled->isArray()) {
			for (const JsonValue &army : uncalled->arrayValue) {
				if (army.isString()) {
					r_result.reinforcements.uncalled.push_back(army.stringValue);
				}
			}
		}
	}

	const JsonValue *destroyed = p_root.get("destroyedObjectives");
	if (destroyed != nullptr && destroyed->isArray()) {
		for (const JsonValue &objective : destroyed->arrayValue) {
			if (objective.isString()) {
				r_result.destroyedObjectives.push_back(objective.stringValue);
			}
		}
	}

	const JsonValue *statistics = p_root.get("tacticalStatistics");
	if (statistics != nullptr && statistics->isString()) {
		r_result.tacticalStatistics = statistics->stringValue;
	}

	const JsonValue *capture_frame = p_root.get("captureFrame");
	if (capture_frame != nullptr && capture_frame->isNumber()) {
		r_result.captureFrame = json_uint(*capture_frame);
	}
	return true;
}

} // namespace

/// The Godot-facing session of the WotrRemaster strategic core.
class WotrRemasterCoreNative : public RefCounted {
	GDCLASS(WotrRemasterCoreNative, RefCounted)

	WotrRemaster::Pack pack;
	WotrRemaster::CampaignState state;
	bool has_pack = false;
	bool has_state = false;
	std::string request_json;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("load_pack", "pack_json"), &WotrRemasterCoreNative::load_pack);
		ClassDB::bind_method(D_METHOD("initial_state", "seed"), &WotrRemasterCoreNative::initial_state);
		ClassDB::bind_method(D_METHOD("resolve_turn", "commands_json"), &WotrRemasterCoreNative::resolve_turn);
		ClassDB::bind_method(D_METHOD("apply_battle_result", "result_json"), &WotrRemasterCoreNative::apply_battle_result);
		ClassDB::bind_method(D_METHOD("state_snapshot"), &WotrRemasterCoreNative::state_snapshot);
		ClassDB::bind_method(D_METHOD("state_hash"), &WotrRemasterCoreNative::state_hash);
		ClassDB::bind_method(D_METHOD("pack_hash"), &WotrRemasterCoreNative::pack_hash);
		ClassDB::bind_method(D_METHOD("save_envelope"), &WotrRemasterCoreNative::save_envelope);
		ClassDB::bind_method(D_METHOD("load_save", "envelope_text"), &WotrRemasterCoreNative::load_save);
		ClassDB::bind_method(D_METHOD("battle_request_json"), &WotrRemasterCoreNative::battle_request_json);
	}

public:
	/// Loads and validates a Campaign Pack document (K2's contract).
	Dictionary load_pack(const String &p_pack_json) {
		const std::string text = to_std(p_pack_json);
		auto result = WotrRemaster::loadPack(text);
		if (!result.ok()) {
			return fail_result(result.err, result.message);
		}
		pack = result.value;
		has_pack = true;
		has_state = false;
		request_json.clear();
		return ok_result();
	}

	/// Builds the initial campaign state from the loaded pack and an explicit seed.
	Dictionary initial_state(int64_t p_seed) {
		if (!has_pack) {
			return fail_result(WotrErrc::InvalidPack, "no campaign pack is loaded");
		}
		auto result = WotrRemaster::initialState(pack, static_cast<uint64_t>(p_seed));
		if (!result.ok()) {
			return fail_result(result.err, result.message);
		}
		state = result.value;
		has_state = true;
		request_json.clear();
		Dictionary out = ok_result();
		out["state_hash"] = to_gd(WotrRemaster::stateHash(state));
		return out;
	}

	/// Resolves a WEGO batch of command intents. The core builds the canonical
	/// envelopes (campaign, turn and checksum) and validates every command.
	Dictionary resolve_turn(const String &p_commands_json) {
		if (!has_state) {
			return fail_result(WotrErrc::InvalidSave, "no campaign state is loaded");
		}
		JsonValue root;
		try {
			root = JsonValue::parse(to_std(p_commands_json));
		} catch (const std::exception &error) {
			return fail_result(WotrErrc::InvalidCommand, std::string("command batch is not valid JSON: ") + error.what());
		}
		if (!root.isArray()) {
			return fail_result(WotrErrc::InvalidCommand, "command batch must be a JSON array");
		}

		std::vector<CommandEnvelope> envelopes;
		for (const JsonValue &item : root.arrayValue) {
			if (!item.isObject()) {
				return fail_result(WotrErrc::InvalidCommand, "a command intent must be a JSON object");
			}
			const JsonValue *player = item.get("playerId");
			const JsonValue *sequence = item.get("clientSequence");
			const JsonValue *command_id = item.get("commandId");
			const JsonValue *army = item.get("armyId");
			const JsonValue *target = item.get("targetRegionId");
			if (player == nullptr || !player->isString() ||
				sequence == nullptr || !sequence->isNumber() ||
				command_id == nullptr || !command_id->isString() ||
				army == nullptr || !army->isString() ||
				target == nullptr || !target->isString()) {
				return fail_result(WotrErrc::InvalidCommand,
						"a command intent needs playerId, clientSequence, commandId, armyId and targetRegionId");
			}
			CommandEnvelope envelope;
			envelope.schemaVersion = 1;
			envelope.campaignId = state.packId;
			envelope.turn = state.turn;
			envelope.playerId = player->stringValue;
			envelope.clientSequence = json_uint(*sequence);
			envelope.commandId = command_id->stringValue;
			envelope.payload.type = CommandType::MoveArmy;
			envelope.payload.move.armyId = army->stringValue;
			envelope.payload.move.targetRegionId = target->stringValue;
			envelope.checksum = WotrRemaster::envelopeChecksum(envelope);
			envelopes.push_back(std::move(envelope));
		}

		auto result = WotrRemaster::resolveCommands(state, envelopes);
		if (!result.ok()) {
			return fail_result(result.err, result.message);
		}
		state = result.value.state;
		request_json.clear();
		Dictionary out = ok_result();
		Array events;
		for (const ResolutionEvent &event : result.value.events) {
			Dictionary item;
			item["type"] = event.type == ResolutionEventType::ArmyMoved ? "army_moved" : "battle_pending";
			item["armyId"] = to_gd(event.armyId);
			item["regionId"] = to_gd(event.regionId);
			item["battleId"] = to_gd(event.battleId);
			events.append(item);
		}
		out["events"] = events;
		if (result.value.battlePending) {
			request_json = request_to_json(result.value.request).canonical();
			out["battle_request"] = json_to_variant(JsonValue::parse(request_json));
		} else {
			out["battle_request"] = Dictionary();
		}
		out["state_hash"] = to_gd(WotrRemaster::stateHash(state));
		return out;
	}

	/// Applies a battle result once. The core rejects a wrong battle id, a
	/// mismatched request checksum, a damaged result checksum and a second
	/// apply, leaving the state byte-identical.
	Dictionary apply_battle_result(const String &p_result_json) {
		if (!has_state) {
			return fail_result(WotrErrc::InvalidSave, "no campaign state is loaded");
		}
		JsonValue root;
		try {
			root = JsonValue::parse(to_std(p_result_json));
		} catch (const std::exception &error) {
			return fail_result(WotrErrc::InvalidResult, std::string("battle result is not valid JSON: ") + error.what());
		}
		BattleResultV1 result;
		std::string error;
		if (!result_from_json(root, result, error)) {
			return fail_result(WotrErrc::InvalidResult, error);
		}
		// The checksum is the core's business. A supplied one must match; a
		// missing one is computed here so a caller cannot forge it.
		const std::string computed = WotrRemaster::battleResultChecksum(result);
		const JsonValue *supplied = root.get("resultChecksum");
		if (supplied != nullptr && supplied->isString() && supplied->stringValue != computed) {
			return fail_result(WotrErrc::DamagedChecksum,
					"battle result checksum does not match the result content");
		}
		result.resultChecksum = computed;

		auto applied = WotrRemaster::applyBattleResult(state, result);
		if (!applied.ok()) {
			return fail_result(applied.err, applied.message);
		}
		state = applied.value;
		request_json.clear();
		Dictionary out = ok_result();
		out["state_hash"] = to_gd(WotrRemaster::stateHash(state));
		out["result_checksum"] = to_gd(computed);
		return out;
	}

	/// The canonical state bytes (read-only view of the campaign).
	String state_snapshot() const {
		if (!has_state) {
			return String();
		}
		return to_gd(WotrRemaster::canonicalState(state));
	}

	String state_hash() const {
		if (!has_state) {
			return String();
		}
		return to_gd(WotrRemaster::stateHash(state));
	}

	String pack_hash() const {
		if (!has_pack) {
			return String();
		}
		return to_gd(WotrRemaster::contentHashOf(pack));
	}

	/// The canonical save envelope (WotrSaveEnvelopeV1).
	String save_envelope() const {
		if (!has_state || !has_pack) {
			return String();
		}
		return to_gd(WotrRemaster::saveCampaign(state, pack));
	}

	/// Loads a save envelope into this session, transactionally.
	Dictionary load_save(const String &p_envelope_text) {
		if (!has_pack) {
			return fail_result(WotrErrc::InvalidPack, "no campaign pack is loaded");
		}
		auto result = WotrRemaster::loadCampaign(to_std(p_envelope_text), pack);
		if (!result.ok()) {
			return fail_result(result.err, result.message);
		}
		state = result.value;
		has_state = true;
		request_json.clear();
		Dictionary out = ok_result();
		out["state_hash"] = to_gd(WotrRemaster::stateHash(state));
		return out;
	}

	/// The pending BattleRequestV1 as canonical JSON (empty when none).
	String battle_request_json() const {
		return to_gd(request_json);
	}
};

static void initialize_wotr_core_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	GDREGISTER_CLASS(WotrRemasterCoreNative);
	register_wotr_battle_bridge();
}

// The extension owns no global state: the campaign state lives in the
// instances. Godot unregisters the class itself, so the terminator does
// nothing (registering the class again here would fail).
static void uninitialize_wotr_core_module(ModuleInitializationLevel p_level) {
	(void)p_level;
}

extern "C" {
GDExtensionBool GDE_EXPORT wotr_core_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address,
		GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
	init_obj.register_initializer(initialize_wotr_core_module);
	init_obj.register_terminator(uninitialize_wotr_core_module);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
	return init_obj.init();
}
}
