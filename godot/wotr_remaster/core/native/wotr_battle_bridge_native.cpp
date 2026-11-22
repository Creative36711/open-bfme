// OpenBFME. GPL-3.0.
// WOTR-1.1-K4 (Godot World Engineer): the native tactical bridge.
//
// The strategic core (engine/src/GameLogic/WotrRemaster, card WOTR-1.1-K2)
// emits an immutable BattleRequestV1. This class hands that request to
// OpenBFME's own War of the Ring battle (LivingWorld::AutoResolveBattle,
// engine/src/GameLogic/LivingWorld/LivingWorldAutoResolveBattle.h) over the
// validated retail RotWK 2.01 / BFME2 1.06 archives, and returns the native
// BattleResultV1 the core applies.
//
// Nothing here is a hand-authored result: every number in the answer is read
// out of the battle the engine ran. The bridge decides nothing either - the
// request checksum and the result checksum are the core's, the winners and
// the survivors are the battle's.
//
// The retail truth (INV-7) is what makes this path a native one: the battle
// reads the retail autoresolve data (AutoResolveWeapon / Armor / Body /
// CombatChain / Leadership / ReinforcementSchedule blocks of RotWK's
// data/ini/livingworldautoresolve*.ini) and the retail ThingTemplate rows
// (hitpoints, experience, armor and weapon sets) of the mounted archives.
// Without a mounted retail install the bridge refuses the request; it never
// falls back to a synthetic fight.

#include "Json.h"
#include "State.h"
#include "wotr_battle_bridge_native.h"

#include "Common/RandomValue.h"
#include "Common/RetailArchivePolicy.h"
#include "GameEngineDevice/Win32Device/Common/Win32BIGFileSystem.h"
#include "GameLogic/LivingWorld/LivingWorldAutoResolveBattle.h"
#include "GameLogic/LivingWorld/LivingWorldUnits.h"
#include "GameLogic/Object/RetailObjectWorld.h"

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <map>
#include <memory>
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

// The result document as a Godot value. The numbers stay integers on the way
// out: the strategic core reads the document back as JSON, and a float in
// this document would change the result the core checksums.
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

uint64_t json_uint(const JsonValue &p_value) {
	if (p_value.type == JsonValue::Type::Int) {
		return static_cast<uint64_t>(p_value.intValue);
	}
	return p_value.uintValue;
}

// The catalog id of a product unit -> the retail stack template that carries
// it in the tactical profile. The profile is product data (the Godot port
// reads it and hands it to the bridge), never a hard-coded table here.
struct ProfileUnit {
	std::string templateName;
	std::map<std::string, std::string> upgradeNames; // product upgrade id -> retail upgrade name
};

struct Profile {
	std::string id;
	std::map<std::string, ProfileUnit> units;
};

// The mounted retail install and the world parsed out of it. Mounting hashes
// every archive (5.3 GiB), so the context is kept for the process' lifetime:
// the first battle pays the mount, the next ones reuse it.
struct RetailContext {
	Win32BIGFileSystem fileSystem;
	std::unique_ptr<RetailObjectWorld> world;
	std::shared_ptr<const LivingWorld::LivingWorldStores> stores;
	LivingWorld::UnitInfoSource units;
	std::string mountError;
	bool mounted = false;
};

RetailContext *retail_context() {
	// Deliberately leaked: the world holds the file system and the mount
	// reports, and their destruction order against Godot's shutdown is not
	// worth the risk for a process-lifetime cache.
	static RetailContext *context = new RetailContext();
	return context;
}

bool mount_retail(RetailContext &r_context, std::string &r_error) {
	if (r_context.mounted) {
		return true;
	}
	const char *rotwk = std::getenv("ROTWK_INSTALL");
	const char *bfme2 = std::getenv("BFME2_INSTALL");
	if (rotwk == nullptr || *rotwk == '\0' || bfme2 == nullptr || *bfme2 == '\0') {
		r_error = "the retail install is not mounted: ROTWK_INSTALL and BFME2_INSTALL must name the validated retail trees";
		return false;
	}
	std::vector<RetailInstall> installs(2);
	installs[0].label = "rotwk";
	installs[0].root = rotwk;
	installs[1].label = "bfme2";
	installs[1].root = bfme2;
	for (RetailInstall &install : installs) {
		const std::string id = install.label == "rotwk" ? "rotwk-201" : "bfme2-106";
		if (!RetailArchivePolicy::loadBuiltin(id, install.policy, &r_error)) {
			return false;
		}
	}
	RetailMountOptions options;
	const RetailMountReport report = MountRetailArchives(r_context.fileSystem, installs, options);
	if (!report.ok) {
		r_error = "the retail archives did not validate";
		for (const std::string &e : report.errors) {
			r_error += ": " + e;
		}
		return false;
	}
	r_context.world = std::make_unique<RetailObjectWorld>(r_context.fileSystem);
	if (!r_context.world->load(&r_error)) {
		return false;
	}
	r_context.stores = r_context.world->livingWorldShared();
	if (r_context.stores == nullptr) {
		r_error = "the retail install has no War of the Ring stores";
		return false;
	}
	r_context.units = LivingWorld::makeUnitInfoSource(*r_context.world);
	r_context.mounted = true;
	return true;
}

// One member of a request participant.
struct RequestMember {
	std::string id;
	std::string templateId;
	uint32_t quantity = 0;
	uint32_t experience = 0;
	std::vector<std::string> persistentUpgrades;
};

struct RequestParticipant {
	std::string id;
	std::string factionId;
	uint8_t side = 0;
	std::vector<RequestMember> members;
};

struct BattleRequest {
	std::string id;
	std::string requestChecksum;
	std::string tacticalProfile;
	std::string tacticalMap;
	std::string regionId;
	uint64_t tacticalSeed = 0;
	std::vector<RequestParticipant> participants;
	std::vector<std::string> heroes;
};

bool read_request(const JsonValue &p_root, BattleRequest &r_request, std::string &r_error) {
	if (!p_root.isObject()) {
		r_error = "the battle request must be a JSON object";
		return false;
	}
	const JsonValue *id = p_root.get("id");
	const JsonValue *checksum = p_root.get("requestChecksum");
	const JsonValue *participants = p_root.get("participants");
	if (id == nullptr || !id->isString() || checksum == nullptr || !checksum->isString() ||
		participants == nullptr || !participants->isArray() || participants->arrayValue.empty()) {
		r_error = "the battle request needs 'id', 'requestChecksum' and a non-empty 'participants' array";
		return false;
	}
	r_request.id = id->stringValue;
	r_request.requestChecksum = checksum->stringValue;
	const JsonValue *profile = p_root.get("tacticalProfile");
	if (profile != nullptr && profile->isString()) {
		r_request.tacticalProfile = profile->stringValue;
	}
	const JsonValue *map = p_root.get("tacticalMap");
	if (map != nullptr && map->isString()) {
		r_request.tacticalMap = map->stringValue;
	}
	const JsonValue *region = p_root.get("regionId");
	if (region != nullptr && region->isString()) {
		r_request.regionId = region->stringValue;
	}
	const JsonValue *seed = p_root.get("tacticalSeed");
	if (seed != nullptr && seed->isNumber()) {
		r_request.tacticalSeed = json_uint(*seed);
	}
	for (const JsonValue &item : participants->arrayValue) {
		const JsonValue *army_id = item.get("id");
		const JsonValue *faction_id = item.get("factionId");
		const JsonValue *side = item.get("side");
		if (army_id == nullptr || !army_id->isString() || faction_id == nullptr || !faction_id->isString() ||
			side == nullptr || !side->isNumber()) {
			r_error = "a battle request participant needs 'id', 'factionId' and 'side'";
			return false;
		}
		RequestParticipant participant;
		participant.id = army_id->stringValue;
		participant.factionId = faction_id->stringValue;
		participant.side = static_cast<uint8_t>(json_uint(*side));
		const JsonValue *members = item.get("members");
		if (members == nullptr || !members->isArray() || members->arrayValue.empty()) {
			r_error = "a battle request participant needs a non-empty 'members' array";
			return false;
		}
		for (const JsonValue &member_item : members->arrayValue) {
			const JsonValue *member_id = member_item.get("id");
			const JsonValue *template_id = member_item.get("templateId");
			const JsonValue *quantity = member_item.get("quantity");
			if (member_id == nullptr || !member_id->isString() || template_id == nullptr || !template_id->isString() ||
				quantity == nullptr || !quantity->isNumber() || json_uint(*quantity) == 0) {
				r_error = "a battle request member needs 'id', 'templateId' and a positive 'quantity'";
				return false;
			}
			RequestMember member;
			member.id = member_id->stringValue;
			member.templateId = template_id->stringValue;
			member.quantity = static_cast<uint32_t>(json_uint(*quantity));
			const JsonValue *experience = member_item.get("experience");
			if (experience != nullptr && experience->isNumber()) {
				member.experience = static_cast<uint32_t>(json_uint(*experience));
			}
			const JsonValue *upgrades = member_item.get("persistentUpgrades");
			if (upgrades != nullptr && upgrades->isArray()) {
				for (const JsonValue &upgrade : upgrades->arrayValue) {
					if (upgrade.isString()) {
						member.persistentUpgrades.push_back(upgrade.stringValue);
					}
				}
			}
			participant.members.push_back(std::move(member));
		}
		r_request.participants.push_back(std::move(participant));
	}
	const JsonValue *heroes = p_root.get("heroes");
	if (heroes != nullptr && heroes->isArray()) {
		for (const JsonValue &hero : heroes->arrayValue) {
			if (hero.isString()) {
				r_request.heroes.push_back(hero.stringValue);
			}
		}
	}
	return true;
}

bool read_profile(const JsonValue &p_root, Profile &r_profile, std::string &r_error) {
	if (!p_root.isObject()) {
		r_error = "the tactical profile must be a JSON object";
		return false;
	}
	const JsonValue *id = p_root.get("profile");
	if (id != nullptr && id->isString()) {
		r_profile.id = id->stringValue;
	}
	const JsonValue *units = p_root.get("units");
	if (units == nullptr || !units->isObject() || units->objectValue.empty()) {
		r_error = "the tactical profile needs a non-empty 'units' object";
		return false;
	}
	for (const auto &kv : units->objectValue) {
		const JsonValue *entry = &kv.second;
		const JsonValue *template_value = entry->get("template");
		if (template_value == nullptr || !template_value->isString() || template_value->stringValue.empty()) {
			r_error = "the tactical profile unit '" + kv.first + "' needs a 'template'";
			return false;
		}
		ProfileUnit unit;
		unit.templateName = template_value->stringValue;
		const JsonValue *upgrades = entry->get("upgrades");
		if (upgrades != nullptr && upgrades->isObject()) {
			for (const auto &upgrade : upgrades->objectValue) {
				if (upgrade.second.isString()) {
					unit.upgradeNames[upgrade.first] = upgrade.second.stringValue;
				}
			}
		}
		r_profile.units[kv.first] = std::move(unit);
	}
	return true;
}

} // namespace

namespace godot {

/// The native tactical bridge: BattleRequestV1 in, native BattleResultV1 out.
void WotrBattleBridge::_bind_methods() {
	ClassDB::bind_method(D_METHOD("run_battle", "request_json", "profile_json"), &WotrBattleBridge::run_battle);
	ClassDB::bind_method(D_METHOD("retail_status"), &WotrBattleBridge::retail_status);
}

/// Runs the native battle of a request and returns the native result.
/// { ok, error_code, error_message, result: {...}, statistics: {...} }
Dictionary WotrBattleBridge::run_battle(const String &p_request_json, const String &p_profile_json) {
		Dictionary out;
		out["ok"] = false;
		out["error_code"] = "InvalidCommand";
		out["error_message"] = "";

		JsonValue request_root;
		JsonValue profile_root;
		try {
			request_root = JsonValue::parse(to_std(p_request_json));
			profile_root = JsonValue::parse(to_std(p_profile_json));
		} catch (const std::exception &error) {
			out["error_message"] = to_gd(std::string("the battle request or the tactical profile is not valid JSON: ") + error.what());
			return out;
		}

		BattleRequest request;
		Profile profile;
		std::string error;
		if (!read_request(request_root, request, error)) {
			out["error_code"] = "InvalidCommand";
			out["error_message"] = to_gd(error);
			return out;
		}
		if (!read_profile(profile_root, profile, error)) {
			out["error_code"] = "InvalidPack";
			out["error_message"] = to_gd(error);
			return out;
		}
		if (!request.tacticalProfile.empty() && !profile.id.empty() && request.tacticalProfile != profile.id) {
			out["error_code"] = "InvalidPack";
			out["error_message"] = to_gd("the request asks for the tactical profile '" + request.tacticalProfile +
					"' but the profile handed to the bridge is '" + profile.id + "'");
			return out;
		}

		RetailContext &context = *retail_context();
		if (!mount_retail(context, error)) {
			out["error_code"] = "ERR_RETAIL_NOT_MOUNTED";
			out["error_message"] = to_gd(error);
			return out;
		}

		// The request's members become the battle's armies: one player record
		// per participant, one army per participant, one unit spec per member.
		std::vector<LivingWorld::AutoResolvePlayerSpec> players;
		std::vector<std::string> armyIds(players.size());
		for (size_t participant_index = 0; participant_index < request.participants.size(); ++participant_index) {
			const RequestParticipant &participant = request.participants[participant_index];
			LivingWorld::AutoResolvePlayerSpec spec;
			spec.player = static_cast<int>(participant_index);
			spec.side = participant.side;
			LivingWorld::AutoResolveArmySpec army;
			army.armyId = static_cast<int>(participant_index);
			army.garrison = false;
			for (size_t member_index = 0; member_index < participant.members.size(); ++member_index) {
				const RequestMember &member = participant.members[member_index];
				const auto profile_unit = profile.units.find(member.templateId);
				if (profile_unit == profile.units.end()) {
					out["error_code"] = "ERR_UNKNOWN_TEMPLATE";
					out["error_message"] = to_gd("the tactical profile has no retail template for '" + member.templateId + "'");
					return out;
				}
				const LivingWorld::UnitInfo info = context.units(profile_unit->second.templateName);
				if (!info.known) {
					out["error_code"] = "ERR_UNKNOWN_TEMPLATE";
					out["error_message"] = to_gd("the retail install has no thing template '" + profile_unit->second.templateName + "'");
					return out;
				}
				if (info.autoResolveUnitType == 9) { // AutoResolveUnit_INVALID
					out["error_code"] = "ERR_UNKNOWN_TEMPLATE";
					out["error_message"] = to_gd("the retail template '" + profile_unit->second.templateName +
							"' carries no autoresolve row and cannot fight");
					return out;
				}
				LivingWorld::AutoResolveUnitSpec unit;
				unit.thingTemplate = profile_unit->second.templateName;
				unit.count = static_cast<int>(member.quantity);
				unit.experience = static_cast<float>(member.experience);
				unit.armyId = static_cast<int>(participant_index);
				unit.memberIndex = static_cast<int>(member_index);
				for (const std::string &upgrade : member.persistentUpgrades) {
					const auto mapped = profile_unit->second.upgradeNames.find(upgrade);
					unit.upgrades.push_back(mapped != profile_unit->second.upgradeNames.end() ? mapped->second : upgrade);
				}
				army.units.push_back(std::move(unit));
			}
			spec.armies.push_back(std::move(army));
			players.push_back(std::move(spec));
		}

		GameLogicRandom random(RandomAlgorithm::ZH_CarryChain);
		random.initGameLogicRandom(static_cast<uint32_t>(request.tacticalSeed & 0xffffffffull), -1);

		LivingWorld::AutoResolveBattle battle(context.stores->autoResolve, context.units, players, random);
		battle.runToEnd();

		// The survivors are read back in the battle's own order (side 0 first)
		// and grouped per army and member of the request.
		std::map<std::string, std::map<std::string, RequestMember>> alive;
		for (const LivingWorld::AutoResolveBattle::Survivor &survivor : battle.survivors()) {
			const int participant_index = survivor.spec.armyId;
			if (participant_index < 0 || static_cast<size_t>(participant_index) >= request.participants.size()) {
				continue;
			}
			const RequestParticipant &participant = request.participants[participant_index];
			const int member_index = survivor.spec.memberIndex;
			if (member_index < 0 || static_cast<size_t>(member_index) >= participant.members.size()) {
				continue;
			}
			const RequestMember &member = participant.members[member_index];
			RequestMember &target = alive[participant.id][member.id];
			target.id = member.id;
			target.templateId = member.templateId;
			target.quantity += static_cast<uint32_t>(survivor.spec.count);
			target.experience = static_cast<uint32_t>(survivor.spec.experience + 0.5f);
			target.persistentUpgrades = survivor.spec.upgrades;
		}

		BattleResultV1 result;
		result.contractVersion = 1;
		result.id = request.id;
		result.requestChecksum = request.requestChecksum;
		const int winner_side = battle.winner();
		if (winner_side < 0) {
			result.completionReason = CompletionReason::Draw;
		} else {
			result.completionReason = CompletionReason::Victory;
			for (const RequestParticipant &participant : request.participants) {
				if (participant.side == static_cast<uint8_t>(winner_side)) {
					result.victor = participant.factionId;
					break;
				}
			}
		}
		for (const RequestParticipant &participant : request.participants) {
			SurvivingArmy army;
			army.id = participant.id;
			const auto army_alive = alive.find(participant.id);
			if (army_alive != alive.end()) {
				for (const auto &member : army_alive->second) {
					SurvivingMember surviving;
					surviving.id = member.second.id;
					surviving.quantity = member.second.quantity;
					surviving.experience = member.second.experience;
					surviving.persistentUpgrades = member.second.persistentUpgrades;
					army.members.push_back(std::move(surviving));
				}
			}
			result.survivors.push_back(std::move(army));
		}
		// A hero of the request that has no survivor left died in the battle.
		for (const std::string &hero : request.heroes) {
			HeroRecord record;
			record.id = hero;
			record.survived = false;
			for (const auto &army : alive) {
				if (army.second.count(hero) != 0) {
					record.survived = true;
				}
			}
			result.heroes.push_back(std::move(record));
		}
		result.reinforcements.called.clear();
		result.reinforcements.uncalled.clear();
		result.destroyedObjectives.clear();
		result.tacticalStatistics = "rounds=" + std::to_string(battle.round()) +
				";kills=" + std::to_string(battle.killCount(0)) + "," + std::to_string(battle.killCount(1)) +
				";draws=" + std::to_string(battle.randomDraws()) +
				";dropped=" + std::to_string(battle.droppedUnits().size());
		result.resultChecksum = battleResultChecksum(result);

		// The result document the strategic core reads back.
		JsonValue document = jobject();
		document.objectValue.emplace("contractVersion", juint(result.contractVersion));
		document.objectValue.emplace("id", jstr(result.id));
		document.objectValue.emplace("requestChecksum", jstr(result.requestChecksum));
		document.objectValue.emplace("completionReason", juint(static_cast<uint64_t>(result.completionReason)));
		if (result.victor.has_value()) {
			document.objectValue.emplace("victor", jstr(*result.victor));
		} else {
			document.objectValue.emplace("victor", JsonValue());
		}
		JsonValue survivors = jarray();
		for (const SurvivingArmy &army : result.survivors) {
			JsonValue item = jobject();
			item.objectValue.emplace("id", jstr(army.id));
			JsonValue members = jarray();
			for (const SurvivingMember &member : army.members) {
				JsonValue member_item = jobject();
				member_item.objectValue.emplace("id", jstr(member.id));
				member_item.objectValue.emplace("quantity", juint(member.quantity));
				member_item.objectValue.emplace("experience", juint(member.experience));
				JsonValue upgrades = jarray();
				for (const std::string &upgrade : member.persistentUpgrades) {
					upgrades.arrayValue.push_back(jstr(upgrade));
				}
				member_item.objectValue.emplace("persistentUpgrades", std::move(upgrades));
				members.arrayValue.push_back(std::move(member_item));
			}
			item.objectValue.emplace("members", std::move(members));
			survivors.arrayValue.push_back(std::move(item));
		}
		document.objectValue.emplace("survivors", std::move(survivors));
		JsonValue heroes = jarray();
		for (const HeroRecord &hero : result.heroes) {
			JsonValue item = jobject();
			item.objectValue.emplace("id", jstr(hero.id));
			item.objectValue.emplace("survived", jbool(hero.survived));
			item.objectValue.emplace("revived", jbool(hero.revived));
			heroes.arrayValue.push_back(std::move(item));
		}
		document.objectValue.emplace("heroes", std::move(heroes));
		JsonValue reinforcements = jobject();
		JsonValue called = jarray();
		JsonValue uncalled = jarray();
		for (const std::string &army : result.reinforcements.called) {
			called.arrayValue.push_back(jstr(army));
		}
		for (const std::string &army : result.reinforcements.uncalled) {
			uncalled.arrayValue.push_back(jstr(army));
		}
		reinforcements.objectValue.emplace("called", std::move(called));
		reinforcements.objectValue.emplace("uncalled", std::move(uncalled));
		document.objectValue.emplace("reinforcements", std::move(reinforcements));
		JsonValue objectives = jarray();
		for (const std::string &objective : result.destroyedObjectives) {
			objectives.arrayValue.push_back(jstr(objective));
		}
		document.objectValue.emplace("destroyedObjectives", std::move(objectives));
		document.objectValue.emplace("tacticalStatistics", jstr(result.tacticalStatistics));
		document.objectValue.emplace("captureFrame", JsonValue());
		document.objectValue.emplace("resultChecksum", jstr(result.resultChecksum));

		out["ok"] = true;
		out["error_code"] = "Ok";
		out["error_message"] = "";
		out["result"] = json_to_variant(document);
		Dictionary statistics;
		statistics["rounds"] = static_cast<int64_t>(battle.round());
		statistics["kills_side0"] = static_cast<int64_t>(battle.killCount(0));
		statistics["kills_side1"] = static_cast<int64_t>(battle.killCount(1));
		statistics["random_draws"] = static_cast<int64_t>(battle.randomDraws());
		statistics["winner_side"] = static_cast<int64_t>(winner_side);
		statistics["entered_armies"] = static_cast<int64_t>(battle.enteredArmies().size());
		statistics["tactical_seed"] = static_cast<int64_t>(request.tacticalSeed);
		statistics["tactical_map"] = to_gd(request.tacticalMap);
		out["statistics"] = statistics;
		return out;
	}

	/// What the retail mount of this process looks like (for the evidence).
	Dictionary WotrBattleBridge::retail_status() {
		Dictionary out;
		RetailContext &context = *retail_context();
		out["mounted"] = context.mounted;
		out["rotwk_install"] = std::getenv("ROTWK_INSTALL") != nullptr ? to_gd(std::getenv("ROTWK_INSTALL")) : String();
		out["bfme2_install"] = std::getenv("BFME2_INSTALL") != nullptr ? to_gd(std::getenv("BFME2_INSTALL")) : String();
		out["templates"] = context.world != nullptr ? static_cast<int64_t>(context.world->things().templateCount()) : static_cast<int64_t>(0);
		return out;
	}

} // namespace godot

void register_wotr_battle_bridge() {
	using namespace godot;
	GDREGISTER_CLASS(WotrBattleBridge);
}
