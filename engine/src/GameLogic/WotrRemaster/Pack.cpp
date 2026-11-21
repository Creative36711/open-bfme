// OpenBFME. GPL-3.0.

#include "Pack.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace WotrRemaster {

namespace {

std::string pathOf(const std::string& section, std::size_t index,
                   const std::string& field) {
    std::ostringstream os;
    os << section << "[" << index << "]." << field;
    return os.str();
}

bool isHexLower(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

bool isHexHash(const std::string& s) {
    return s.size() == 64 && std::all_of(s.begin(), s.end(), isHexLower);
}

// Get a required object member of the expected type, or fail with context.
WotrResult<JsonValue> requireMember(const JsonValue& obj, const std::string& key,
                                    JsonValue::Type type, const std::string& ctx) {
    const JsonValue* v = obj.get(key);
    if (v == nullptr) {
        return fail<JsonValue>(WotrErrc::InvalidPack,
                               ctx + ": missing required field '" + key + "'");
    }
    if (v->type != type) {
        return fail<JsonValue>(WotrErrc::InvalidPack,
                               ctx + ": field '" + key + "' has wrong type");
    }
    return ok(*v);
}

WotrResult<std::string> requireString(const JsonValue& obj, const std::string& key,
                                      const std::string& ctx) {
    auto r = requireMember(obj, key, JsonValue::Type::String, ctx);
    if (!r.ok()) return fail<std::string>(r.err, r.message);
    return ok(r.value.stringValue);
}

WotrResult<std::uint64_t> requireUInt(const JsonValue& obj, const std::string& key,
                                      const std::string& ctx) {
    auto r = requireMember(obj, key, JsonValue::Type::UInt, ctx);
    if (!r.ok()) {
        // A number that is not a clean UInt (negative or too large) is still
        // the wrong kind of value for counts and versions.
        auto r2 = requireMember(obj, key, JsonValue::Type::Int, ctx);
        if (!r2.ok()) return fail<std::uint64_t>(r2.err, r2.message);
        if (r2.value.intValue < 0) {
            return fail<std::uint64_t>(WotrErrc::InvalidPack,
                                       ctx + ": field '" + key + "' must be >= 0");
        }
        return ok(static_cast<std::uint64_t>(r2.value.intValue));
    }
    return ok(r.value.uintValue);
}

WotrResult<std::vector<std::string>> requireStringArray(const JsonValue& obj,
                                                        const std::string& key,
                                                        const std::string& ctx) {
    auto r = requireMember(obj, key, JsonValue::Type::Array, ctx);
    if (!r.ok()) return fail<std::vector<std::string>>(r.err, r.message);
    std::vector<std::string> out;
    out.reserve(r.value.arrayValue.size());
    for (std::size_t i = 0; i < r.value.arrayValue.size(); ++i) {
        const JsonValue& item = r.value.arrayValue[i];
        if (item.type != JsonValue::Type::String) {
            return fail<std::vector<std::string>>(
                WotrErrc::InvalidPack,
                ctx + ": field '" + key + "[" + std::to_string(i) + "]' is not a string");
        }
        out.push_back(item.stringValue);
    }
    return ok(out);
}

} // namespace

bool validId(const std::string& id) {
    if (id.empty()) return false;
    for (char c : id) {
        if (static_cast<unsigned char>(c) < 0x20) return false;
        if (c == '/' || c == '\\' || c == ':' || c == '.' || c == '#' ||
            c == '%' || c == '?' || c == ' ')
            return false;
    }
    return true;
}

bool referenceIsStable(const std::string& ref) {
    if (ref.empty()) return false;
    // Absolute, author-machine paths are never stable.
    if (ref[0] == '/' || ref[0] == '\\') return false;
    if (ref.size() >= 2 && ref[1] == ':' &&
        ((ref[0] >= 'a' && ref[0] <= 'z') || (ref[0] >= 'A' && ref[0] <= 'Z')))
        return false;
    // ".." path segments make resolution dependent on where the pack is mounted.
    if (ref == "..") return false;
    for (std::size_t i = 0; i + 1 < ref.size(); ++i) {
        if ((ref[i] == '/' || ref[i] == '\\') && ref[i + 1] == '.' &&
            (i + 2 == ref.size() || ref[i + 2] == '/' || ref[i + 2] == '\\'))
            return false;
    }
    // Explicit reference kinds are stable: pack:, retail:, custom:.
    if (ref.compare(0, 5, "pack:") == 0 || ref.compare(0, 7, "retail:") == 0 ||
        ref.compare(0, 7, "custom:") == 0)
        return true;
    // Bare ids (pack-scoped references) are stable.
    return validId(ref);
}

WotrResult<Pack> loadPack(std::string jsonText) {
    Pack pack;
    JsonValue root;
    try {
        root = JsonValue::parse(jsonText);
    } catch (const JsonParseError& e) {
        return fail<Pack>(WotrErrc::InvalidPack, e.what());
    }
    if (!root.isObject()) {
        return fail<Pack>(WotrErrc::InvalidPack,
                          "pack document root is not an object");
    }

    const std::string packCtx = "pack";

    // ---- manifest ---------------------------------------------------------
    const JsonValue* manifestNode = root.get("pack");
    if (manifestNode == nullptr || !manifestNode->isObject()) {
        return fail<Pack>(WotrErrc::MissingTacticalProfile,
                          "pack: missing 'pack' manifest object");
    }
    const JsonValue& m = *manifestNode;

    auto idRes = requireString(m, "id", packCtx);
    if (!idRes.ok()) return fail<Pack>(idRes.err, idRes.message);
    pack.manifest.id = idRes.value;
    if (!validId(pack.manifest.id)) {
        return fail<Pack>(WotrErrc::InvalidPack,
                          "pack: field 'id' is not a stable id: '" +
                              pack.manifest.id + "'");
    }

    auto versionRes = requireUInt(m, "version", packCtx);
    if (!versionRes.ok()) return fail<Pack>(versionRes.err, versionRes.message);
    if (versionRes.value < 1 || versionRes.value > 0xffffffffu) {
        return fail<Pack>(WotrErrc::InvalidPack,
                          "pack: field 'version' must be >= 1 and <= 4294967295");
    }
    pack.manifest.version = static_cast<std::uint32_t>(versionRes.value);

    // tactical_profile: missing -> MissingTacticalProfile, wrong type ->
    // InvalidPack, unsupported value -> MissingTacticalProfile.
    {
        const JsonValue* pv = m.get("tactical_profile");
        if (pv == nullptr) {
            return fail<Pack>(WotrErrc::MissingTacticalProfile,
                              "pack: missing required field 'tactical_profile'");
        }
        if (pv->type != JsonValue::Type::String) {
            return fail<Pack>(WotrErrc::InvalidPack,
                              "pack: field 'tactical_profile' has wrong type");
        }
        pack.manifest.tacticalProfile = pv->stringValue;
        if (pack.manifest.tacticalProfile != kVanillaRotWk201Profile) {
            return fail<Pack>(WotrErrc::MissingTacticalProfile,
                              "pack: tactical_profile '" +
                                  pack.manifest.tacticalProfile +
                                  "' is not supported (expected '" +
                                  std::string(kVanillaRotWk201Profile) + "')");
        }
    }

    // tactical_map: missing -> MissingTacticalMap, wrong type -> InvalidPack.
    {
        const JsonValue* mv = m.get("tactical_map");
        if (mv == nullptr) {
            return fail<Pack>(WotrErrc::MissingTacticalMap,
                              "pack: missing required field 'tactical_map'");
        }
        if (mv->type != JsonValue::Type::String) {
            return fail<Pack>(WotrErrc::InvalidPack,
                              "pack: field 'tactical_map' has wrong type");
        }
        pack.manifest.tacticalMap = mv->stringValue;
        if (pack.manifest.tacticalMap.empty()) {
            return fail<Pack>(WotrErrc::MissingTacticalMap,
                              "pack: tactical_map is empty");
        }
    }
    if (pack.manifest.tacticalMap[0] == '/' ||
        (pack.manifest.tacticalMap.size() >= 2 &&
         pack.manifest.tacticalMap[1] == ':')) {
        return fail<Pack>(WotrErrc::AbsoluteReference,
                          "pack: tactical_map is an absolute path: '" +
                              pack.manifest.tacticalMap + "'");
    }
    if (!referenceIsStable(pack.manifest.tacticalMap)) {
        return fail<Pack>(WotrErrc::UnstableReference,
                          "pack: tactical_map is an unstable reference: '" +
                              pack.manifest.tacticalMap + "'");
    }

    if (const JsonValue* ch = m.get("contentHash")) {
        if (ch->type != JsonValue::Type::String || !isHexHash(ch->stringValue)) {
            return fail<Pack>(WotrErrc::InvalidPack,
                              "pack: field 'contentHash' must be a lowercase sha-256 hex string");
        }
        pack.manifest.contentHash = ch->stringValue;
    }

    // ---- regions ----------------------------------------------------------
    auto regionsRes = requireMember(root, "regions", JsonValue::Type::Array, "root");
    if (!regionsRes.ok()) return fail<Pack>(regionsRes.err, regionsRes.message);
    for (std::size_t i = 0; i < regionsRes.value.arrayValue.size(); ++i) {
        const JsonValue& item = regionsRes.value.arrayValue[i];
        if (!item.isObject()) {
            return fail<Pack>(WotrErrc::InvalidPack,
                              pathOf("regions", i, "") + ": region is not an object");
        }
        std::string ctx = pathOf("regions", i, "");
        PackRegion reg;
        auto ridRes = requireString(item, "id", ctx);
        if (!ridRes.ok()) return fail<Pack>(ridRes.err, ridRes.message);
        reg.id = ridRes.value;
        if (!validId(reg.id)) {
            return fail<Pack>(WotrErrc::InvalidPack,
                              ctx + ": field 'id' is not a stable id: '" + reg.id + "'");
        }
        auto nameRes = requireString(item, "name", ctx);
        if (!nameRes.ok()) return fail<Pack>(nameRes.err, nameRes.message);
        reg.name = nameRes.value;
        if (reg.name.empty()) {
            return fail<Pack>(WotrErrc::InvalidPack,
                              ctx + ": field 'name' is empty");
        }
        auto conRes = requireStringArray(item, "connections", ctx);
        if (!conRes.ok()) return fail<Pack>(conRes.err, conRes.message);
        reg.connections = conRes.value;
        pack.regions.push_back(std::move(reg));
    }

    // ---- factions ---------------------------------------------------------
    auto factionsRes = requireMember(root, "factions", JsonValue::Type::Array, "root");
    if (!factionsRes.ok()) return fail<Pack>(factionsRes.err, factionsRes.message);
    for (std::size_t i = 0; i < factionsRes.value.arrayValue.size(); ++i) {
        const JsonValue& item = factionsRes.value.arrayValue[i];
        if (!item.isObject()) {
            return fail<Pack>(WotrErrc::InvalidPack,
                              pathOf("factions", i, "") + ": faction is not an object");
        }
        std::string ctx = pathOf("factions", i, "");
        PackFaction fac;
        auto fidRes = requireString(item, "id", ctx);
        if (!fidRes.ok()) return fail<Pack>(fidRes.err, fidRes.message);
        fac.id = fidRes.value;
        if (!validId(fac.id)) {
            return fail<Pack>(WotrErrc::InvalidPack,
                              ctx + ": field 'id' is not a stable id: '" + fac.id + "'");
        }
        auto fnameRes = requireString(item, "name", ctx);
        if (!fnameRes.ok()) return fail<Pack>(fnameRes.err, fnameRes.message);
        fac.name = fnameRes.value;
        if (fac.name.empty()) {
            return fail<Pack>(WotrErrc::InvalidPack,
                              ctx + ": field 'name' is empty");
        }
        pack.factions.push_back(std::move(fac));
    }

    // ---- armies -----------------------------------------------------------
    auto armiesRes = requireMember(root, "armies", JsonValue::Type::Array, "root");
    if (!armiesRes.ok()) return fail<Pack>(armiesRes.err, armiesRes.message);
    for (std::size_t i = 0; i < armiesRes.value.arrayValue.size(); ++i) {
        const JsonValue& item = armiesRes.value.arrayValue[i];
        if (!item.isObject()) {
            return fail<Pack>(WotrErrc::InvalidPack,
                              pathOf("armies", i, "") + ": army is not an object");
        }
        std::string ctx = pathOf("armies", i, "");
        PackArmy army;
        auto aidRes = requireString(item, "id", ctx);
        if (!aidRes.ok()) return fail<Pack>(aidRes.err, aidRes.message);
        army.id = aidRes.value;
        if (!validId(army.id)) {
            return fail<Pack>(WotrErrc::InvalidPack,
                              ctx + ": field 'id' is not a stable id: '" + army.id + "'");
        }
        auto fid2Res = requireString(item, "factionId", ctx);
        if (!fid2Res.ok()) return fail<Pack>(fid2Res.err, fid2Res.message);
        army.factionId = fid2Res.value;
        auto rid2Res = requireString(item, "regionId", ctx);
        if (!rid2Res.ok()) return fail<Pack>(rid2Res.err, rid2Res.message);
        army.regionId = rid2Res.value;
        auto membersRes =
            requireMember(item, "members", JsonValue::Type::Array, ctx);
        if (!membersRes.ok()) return fail<Pack>(membersRes.err, membersRes.message);
        for (std::size_t j = 0; j < membersRes.value.arrayValue.size(); ++j) {
            const JsonValue& mem = membersRes.value.arrayValue[j];
            if (!mem.isObject()) {
                return fail<Pack>(
                    WotrErrc::InvalidPack,
                    ctx + ": field 'members[" + std::to_string(j) + "]' is not an object");
            }
            std::string mctx = ctx + ".members[" + std::to_string(j) + "]";
            PackMember member;
            auto midRes = requireString(mem, "id", mctx);
            if (!midRes.ok()) return fail<Pack>(midRes.err, midRes.message);
            member.id = midRes.value;
            if (!validId(member.id)) {
                return fail<Pack>(WotrErrc::InvalidPack,
                                  mctx + ": field 'id' is not a stable id: '" +
                                      member.id + "'");
            }
            auto kindRes = requireString(mem, "kind", mctx);
            if (!kindRes.ok()) return fail<Pack>(kindRes.err, kindRes.message);
            member.kind = kindRes.value;
            if (member.kind.empty()) {
                return fail<Pack>(WotrErrc::InvalidPack,
                                  mctx + ": field 'kind' is empty");
            }
            auto qtyRes = requireUInt(mem, "quantity", mctx);
            if (!qtyRes.ok()) return fail<Pack>(qtyRes.err, qtyRes.message);
            member.quantity = static_cast<std::uint32_t>(qtyRes.value);
            auto xpRes = requireUInt(mem, "experience", mctx);
            if (!xpRes.ok()) return fail<Pack>(xpRes.err, xpRes.message);
            member.experience = static_cast<std::uint32_t>(xpRes.value);
            auto ur = requireMember(mem, "persistentUpgrades",
                                    JsonValue::Type::Array, mctx);
            if (!ur.ok()) return fail<Pack>(ur.err, ur.message);
            for (const JsonValue& up : ur.value.arrayValue) {
                if (up.type != JsonValue::Type::String) {
                    return fail<Pack>(WotrErrc::InvalidPack,
                                      mctx + ": field 'persistentUpgrades[*]' must be strings");
                }
                member.persistentUpgrades.push_back(up.stringValue);
            }
            army.members.push_back(std::move(member));
        }
        pack.armies.push_back(std::move(army));
    }

    // ---- semantic validation ----------------------------------------------
    // unique ids across sections
    {
        std::unordered_set<std::string> seen;
        for (const auto& reg : pack.regions) {
            if (!seen.insert(reg.id).second) {
                return fail<Pack>(WotrErrc::DuplicateId,
                                  "duplicate region id: '" + reg.id + "'");
            }
        }
        for (const auto& fac : pack.factions) {
            if (!seen.insert(fac.id).second) {
                return fail<Pack>(WotrErrc::DuplicateId,
                                  "duplicate faction id: '" + fac.id + "'");
            }
        }
        for (const auto& army : pack.armies) {
            if (!seen.insert(army.id).second) {
                return fail<Pack>(WotrErrc::DuplicateId,
                                  "duplicate army id: '" + army.id + "'");
            }
        }
    }
    // topology: connections reference known regions, no duplicates, symmetric
    {
        std::unordered_set<std::string> regionIds;
        for (const auto& reg : pack.regions) regionIds.insert(reg.id);
        for (const auto& reg : pack.regions) {
            std::unordered_set<std::string> seenTargets;
            for (const auto& target : reg.connections) {
                if (target == reg.id) {
                    return fail<Pack>(WotrErrc::BrokenTopology,
                                      "region '" + reg.id +
                                          "' connects to itself");
                }
                if (regionIds.find(target) == regionIds.end()) {
                    return fail<Pack>(WotrErrc::BrokenTopology,
                                      "region '" + reg.id +
                                          "' connects to unknown region '" + target + "'");
                }
                if (!seenTargets.insert(target).second) {
                    return fail<Pack>(WotrErrc::BrokenTopology,
                                      "region '" + reg.id +
                                          "' has duplicate connection to '" + target + "'");
                }
            }
        }
        // symmetry: A connects to B => B connects to A
        for (const auto& reg : pack.regions) {
            for (const auto& target : reg.connections) {
                bool found = false;
                for (const auto& other : pack.regions) {
                    if (other.id == target) {
                        for (const auto& back : other.connections) {
                            if (back == reg.id) { found = true; break; }
                        }
                        break;
                    }
                }
                if (!found) {
                    return fail<Pack>(WotrErrc::BrokenTopology,
                                      "topology is not symmetric: '" + reg.id +
                                          "' -> '" + target +
                                          "' is missing the reverse link");
                }
            }
        }
    }
    // army references and member uniqueness
    {
        std::unordered_set<std::string> factionIds, regionIds;
        for (const auto& f : pack.factions) factionIds.insert(f.id);
        for (const auto& r : pack.regions) regionIds.insert(r.id);
        for (const auto& army : pack.armies) {
            if (factionIds.find(army.factionId) == factionIds.end()) {
                return fail<Pack>(WotrErrc::UnknownEntity,
                                  "army '" + army.id +
                                      "' references unknown faction '" + army.factionId + "'");
            }
            if (regionIds.find(army.regionId) == regionIds.end()) {
                return fail<Pack>(WotrErrc::UnknownEntity,
                                  "army '" + army.id +
                                      "' references unknown region '" + army.regionId + "'");
            }
            std::unordered_set<std::string> memberIds;
            for (const auto& mem : army.members) {
                if (!validId(mem.id)) {
                    return fail<Pack>(WotrErrc::InvalidPack,
                                      "army '" + army.id +
                                          "' member has invalid id: '" + mem.id + "'");
                }
                if (!memberIds.insert(mem.id).second) {
                    return fail<Pack>(WotrErrc::DuplicateId,
                                      "army '" + army.id +
                                          "' has duplicate member id: '" + mem.id + "'");
                }
            }
        }
    }

    // content hash (self-excluding: computed over the pack without this field)
    if (pack.manifest.contentHash.has_value()) {
        PackHash actual = contentHashOf(pack);
        if (actual != *pack.manifest.contentHash) {
            return fail<Pack>(WotrErrc::ContentHashMismatch,
                              "pack content hash mismatch: manifest declares " +
                                  *pack.manifest.contentHash +
                                  ", computed " + actual);
        }
    }

    return ok(pack);
}

std::string canonicalPack(const Pack& pack) {
    JsonValue root;
    root.type = JsonValue::Type::Object;

    JsonValue manifest;
    manifest.type = JsonValue::Type::Object;
    {
        JsonValue v; v.type = JsonValue::Type::String; v.stringValue = pack.manifest.id;
        manifest.objectValue.emplace("id", std::move(v));
        JsonValue v2; v2.type = JsonValue::Type::UInt; v2.uintValue = pack.manifest.version;
        manifest.objectValue.emplace("version", std::move(v2));
        JsonValue v3; v3.type = JsonValue::Type::String;
        v3.stringValue = pack.manifest.tacticalProfile;
        manifest.objectValue.emplace("tactical_profile", std::move(v3));
        JsonValue v4; v4.type = JsonValue::Type::String;
        v4.stringValue = pack.manifest.tacticalMap;
        manifest.objectValue.emplace("tactical_map", std::move(v4));
        // contentHash is deliberately omitted: it is self-excluding.
    }
    root.objectValue.emplace("pack", std::move(manifest));

    // Regions sorted by id so canonical bytes never depend on source order.
    std::vector<PackRegion> regions = pack.regions;
    std::sort(regions.begin(), regions.end(),
              [](const PackRegion& a, const PackRegion& b) { return a.id < b.id; });
    JsonValue regionsJson; regionsJson.type = JsonValue::Type::Array;
    for (const auto& reg : regions) {
        JsonValue item; item.type = JsonValue::Type::Object;
        {
            JsonValue v; v.type = JsonValue::Type::String; v.stringValue = reg.id;
            item.objectValue.emplace("id", std::move(v));
            JsonValue v2; v2.type = JsonValue::Type::String; v2.stringValue = reg.name;
            item.objectValue.emplace("name", std::move(v2));
            JsonValue con; con.type = JsonValue::Type::Array;
            std::vector<std::string> conSorted = reg.connections;
            std::sort(conSorted.begin(), conSorted.end());
            conSorted.erase(std::unique(conSorted.begin(), conSorted.end()), conSorted.end());
            for (const auto& c : conSorted) {
                JsonValue cv; cv.type = JsonValue::Type::String; cv.stringValue = c;
                con.arrayValue.push_back(std::move(cv));
            }
            item.objectValue.emplace("connections", std::move(con));
        }
        regionsJson.arrayValue.push_back(std::move(item));
    }
    root.objectValue.emplace("regions", std::move(regionsJson));

    std::vector<PackFaction> factions = pack.factions;
    std::sort(factions.begin(), factions.end(),
              [](const PackFaction& a, const PackFaction& b) { return a.id < b.id; });
    JsonValue factionsJson; factionsJson.type = JsonValue::Type::Array;
    for (const auto& f : factions) {
        JsonValue item; item.type = JsonValue::Type::Object;
        {
            JsonValue v; v.type = JsonValue::Type::String; v.stringValue = f.id;
            item.objectValue.emplace("id", std::move(v));
            JsonValue v2; v2.type = JsonValue::Type::String; v2.stringValue = f.name;
            item.objectValue.emplace("name", std::move(v2));
        }
        factionsJson.arrayValue.push_back(std::move(item));
    }
    root.objectValue.emplace("factions", std::move(factionsJson));

    std::vector<PackArmy> armies = pack.armies;
    std::sort(armies.begin(), armies.end(),
              [](const PackArmy& a, const PackArmy& b) { return a.id < b.id; });
    JsonValue armiesJson; armiesJson.type = JsonValue::Type::Array;
    for (const auto& a : armies) {
        JsonValue item; item.type = JsonValue::Type::Object;
        {
            JsonValue v; v.type = JsonValue::Type::String; v.stringValue = a.id;
            item.objectValue.emplace("id", std::move(v));
            JsonValue v2; v2.type = JsonValue::Type::String; v2.stringValue = a.factionId;
            item.objectValue.emplace("factionId", std::move(v2));
            JsonValue v3; v3.type = JsonValue::Type::String; v3.stringValue = a.regionId;
            item.objectValue.emplace("regionId", std::move(v3));
            std::vector<PackMember> members = a.members;
            std::sort(members.begin(), members.end(),
                      [](const PackMember& x, const PackMember& y) { return x.id < y.id; });
            JsonValue memJson; memJson.type = JsonValue::Type::Array;
            for (const auto& mem : members) {
                JsonValue mitem; mitem.type = JsonValue::Type::Object;
                {
                    JsonValue v; v.type = JsonValue::Type::String; v.stringValue = mem.id;
                    mitem.objectValue.emplace("id", std::move(v));
                    JsonValue v2; v2.type = JsonValue::Type::String; v2.stringValue = mem.kind;
                    mitem.objectValue.emplace("kind", std::move(v2));
                    JsonValue v3; v3.type = JsonValue::Type::UInt; v3.uintValue = mem.quantity;
                    mitem.objectValue.emplace("quantity", std::move(v3));
                    JsonValue v4; v4.type = JsonValue::Type::UInt; v4.uintValue = mem.experience;
                    mitem.objectValue.emplace("experience", std::move(v4));
                    JsonValue up; up.type = JsonValue::Type::Array;
                    std::vector<UpgradeId> ups = mem.persistentUpgrades;
                    std::sort(ups.begin(), ups.end());
                    ups.erase(std::unique(ups.begin(), ups.end()), ups.end());
                    for (const auto& u : ups) {
                        JsonValue uv; uv.type = JsonValue::Type::String; uv.stringValue = u;
                        up.arrayValue.push_back(std::move(uv));
                    }
                    mitem.objectValue.emplace("persistentUpgrades", std::move(up));
                }
                memJson.arrayValue.push_back(std::move(mitem));
            }
            item.objectValue.emplace("members", std::move(memJson));
        }
        armiesJson.arrayValue.push_back(std::move(item));
    }
    root.objectValue.emplace("armies", std::move(armiesJson));

    return root.canonical();
}

PackHash contentHashOf(const Pack& pack) {
    return Sha256::hashHex(canonicalPack(pack));
}

} // namespace WotrRemaster
