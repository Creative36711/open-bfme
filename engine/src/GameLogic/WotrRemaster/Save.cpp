// OpenBFME. GPL-3.0.

#include "Save.h"

#include <algorithm>
#include <sstream>

namespace WotrRemaster {

namespace {

JsonValue jstr(const std::string& v) {
    JsonValue x;
    x.type = JsonValue::Type::String;
    x.stringValue = v;
    return x;
}

JsonValue juint(std::uint64_t v) {
    JsonValue x;
    x.type = JsonValue::Type::UInt;
    x.uintValue = v;
    return x;
}

JsonValue jnull() {
    JsonValue x;
    x.type = JsonValue::Type::Null;
    return x;
}

JsonValue jarray() {
    JsonValue x;
    x.type = JsonValue::Type::Array;
    return x;
}

JsonValue jobject() {
    JsonValue x;
    x.type = JsonValue::Type::Object;
    return x;
}

bool isLowerHex64(const std::string& s) {
    if (s.size() != 64) return false;
    for (char c : s) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

// When raw byte corruption inside a canonically serialized envelope breaks JSON
// quoting/escaping (for instance flipping a backslash inside stateBytes), the
// outer {"checksum":"<64-hex>",...} frame still carries the sealed digest and
// the reconstructed payload will fail SHA-256 verification.
bool hasDamagedCanonicalEnvelopeChecksum(const std::string& text) {
    static const std::string prefix = "{\"checksum\":\"";
    if (text.size() < prefix.size() + 64 + 3) return false;
    if (text.compare(0, prefix.size(), prefix) != 0) return false;
    if (text.back() != '}') return false;
    const std::string declared = text.substr(prefix.size(), 64);
    if (!isLowerHex64(declared)) return false;
    if (text.compare(prefix.size() + 64, 2, "\",") != 0) return false;
    const std::string rawPayload = "{" + text.substr(prefix.size() + 64 + 2);
    return Sha256::hashHex(rawPayload) != declared;
}

} // namespace

std::string saveEnvelopePayload(const WotrSaveEnvelopeV1& e) {
    JsonValue env = jobject();
    env.objectValue.emplace("envelopeVersion", juint(e.envelopeVersion));
    env.objectValue.emplace("contractVersion", juint(e.contractVersion));
    env.objectValue.emplace("packId", jstr(e.packId));
    env.objectValue.emplace("packHash", jstr(e.packHash));
    env.objectValue.emplace("tacticalProfile", jstr(e.tacticalProfile));
    env.objectValue.emplace("stateBytes", jstr(e.stateBytes));
    env.objectValue.emplace("stateHash", jstr(e.stateHash));
    env.objectValue.emplace("commandCursor", juint(e.commandCursor));
    env.objectValue.emplace("rngMetadata", jstr(e.rngMetadata));
    return env.canonical();
}

std::string serializeSaveEnvelope(const WotrSaveEnvelopeV1& e) {
    JsonValue env = jobject();
    env.objectValue.emplace("envelopeVersion", juint(e.envelopeVersion));
    env.objectValue.emplace("contractVersion", juint(e.contractVersion));
    env.objectValue.emplace("packId", jstr(e.packId));
    env.objectValue.emplace("packHash", jstr(e.packHash));
    env.objectValue.emplace("tacticalProfile", jstr(e.tacticalProfile));
    env.objectValue.emplace("stateBytes", jstr(e.stateBytes));
    env.objectValue.emplace("stateHash", jstr(e.stateHash));
    env.objectValue.emplace("commandCursor", juint(e.commandCursor));
    env.objectValue.emplace("rngMetadata", jstr(e.rngMetadata));
    env.objectValue.emplace("checksum", jstr(e.checksum));
    return env.canonical();
}

std::string saveCampaign(const CampaignState& s, const Pack& pack) {
    WotrSaveEnvelopeV1 e;
    e.envelopeVersion = 1;
    e.contractVersion = 1;
    e.packId = pack.manifest.id;
    e.packHash = contentHashOf(pack);
    e.tacticalProfile = pack.manifest.tacticalProfile;
    e.stateBytes = canonicalState(s);
    e.stateHash = stateHash(s);
    e.commandCursor = s.acceptedCommands.size();
    e.rngMetadata = "{}";
    e.checksum = Sha256::hashHex(saveEnvelopePayload(e));
    return serializeSaveEnvelope(e);
}

WotrResult<WotrSaveEnvelopeV1> parseSaveEnvelope(const std::string& text) {
    JsonValue root;
    try {
        root = JsonValue::parse(text);
    } catch (const JsonParseError& ex) {
        if (hasDamagedCanonicalEnvelopeChecksum(text)) {
            return fail<WotrSaveEnvelopeV1>(WotrErrc::DamagedSaveChecksum,
                                            "save envelope checksum does not verify");
        }
        return fail<WotrSaveEnvelopeV1>(WotrErrc::InvalidSave, ex.what());
    }
    if (!root.isObject()) {
        return fail<WotrSaveEnvelopeV1>(WotrErrc::InvalidSave,
                                        "save envelope is not an object");
    }

    WotrSaveEnvelopeV1 e;

    auto reqUInt = [&](const char* key) -> WotrResult<std::uint64_t> {
        const JsonValue* v = root.get(key);
        if (v == nullptr ||
            (v->type != JsonValue::Type::UInt && v->type != JsonValue::Type::Int)) {
            return fail<std::uint64_t>(WotrErrc::InvalidSave,
                                       std::string("missing or wrong-typed field '") + key + "'");
        }
        if (v->type == JsonValue::Type::UInt) return ok(v->uintValue);
        if (v->intValue < 0) return fail<std::uint64_t>(WotrErrc::InvalidSave,
                                                        std::string("field '") + key + "' must be >= 0");
        return ok(static_cast<std::uint64_t>(v->intValue));
    };
    auto reqStr = [&](const char* key) -> WotrResult<std::string> {
        const JsonValue* v = root.get(key);
        if (v == nullptr || v->type != JsonValue::Type::String) {
            return fail<std::string>(WotrErrc::InvalidSave,
                                     std::string("missing or wrong-typed field '") + key + "'");
        }
        return ok(v->stringValue);
    };

    auto verRes = reqUInt("envelopeVersion");
    if (!verRes.ok()) return fail<WotrSaveEnvelopeV1>(verRes.err, verRes.message);
    e.envelopeVersion = static_cast<std::uint32_t>(verRes.value);
    if (e.envelopeVersion != 1) {
        return fail<WotrSaveEnvelopeV1>(WotrErrc::UnknownSaveVersion,
                                        "unsupported save envelope version " +
                                            std::to_string(e.envelopeVersion));
    }

    auto cvRes = reqUInt("contractVersion");
    if (!cvRes.ok()) return fail<WotrSaveEnvelopeV1>(cvRes.err, cvRes.message);
    e.contractVersion = static_cast<std::uint32_t>(cvRes.value);
    if (e.contractVersion != 1) {
        return fail<WotrSaveEnvelopeV1>(WotrErrc::UnknownSaveVersion,
                                        "unsupported save contract version " +
                                            std::to_string(e.contractVersion));
    }

    auto packIdRes = reqStr("packId");
    if (!packIdRes.ok()) return fail<WotrSaveEnvelopeV1>(packIdRes.err, packIdRes.message);
    e.packId = packIdRes.value;

    auto packHashRes = reqStr("packHash");
    if (!packHashRes.ok()) return fail<WotrSaveEnvelopeV1>(packHashRes.err, packHashRes.message);
    e.packHash = packHashRes.value;

    auto profileRes = reqStr("tacticalProfile");
    if (!profileRes.ok()) return fail<WotrSaveEnvelopeV1>(profileRes.err, profileRes.message);
    e.tacticalProfile = profileRes.value;

    auto stateBytesRes = reqStr("stateBytes");
    if (!stateBytesRes.ok()) return fail<WotrSaveEnvelopeV1>(stateBytesRes.err, stateBytesRes.message);
    e.stateBytes = stateBytesRes.value;

    auto stateHashRes = reqStr("stateHash");
    if (!stateHashRes.ok()) return fail<WotrSaveEnvelopeV1>(stateHashRes.err, stateHashRes.message);
    e.stateHash = stateHashRes.value;

    auto cursorRes = reqUInt("commandCursor");
    if (!cursorRes.ok()) return fail<WotrSaveEnvelopeV1>(cursorRes.err, cursorRes.message);
    e.commandCursor = cursorRes.value;

    auto rngRes = reqStr("rngMetadata");
    if (!rngRes.ok()) return fail<WotrSaveEnvelopeV1>(rngRes.err, rngRes.message);
    e.rngMetadata = rngRes.value;

    auto checksumRes = reqStr("checksum");
    if (!checksumRes.ok()) return fail<WotrSaveEnvelopeV1>(checksumRes.err, checksumRes.message);
    e.checksum = checksumRes.value;

    // Verify checksum over the payload.
    WotrSaveEnvelopeV1 payloadCopy = e;
    payloadCopy.checksum.clear();
    std::string expected = Sha256::hashHex(saveEnvelopePayload(payloadCopy));
    if (expected != e.checksum) {
        return fail<WotrSaveEnvelopeV1>(WotrErrc::DamagedSaveChecksum,
                                        "save envelope checksum does not verify");
    }

    return ok(e);
}

WotrResult<CampaignState> loadCampaign(const std::string& text, const Pack& pack) {
    auto envRes = parseSaveEnvelope(text);
    if (!envRes.ok()) return fail<CampaignState>(envRes.err, envRes.message);
    const WotrSaveEnvelopeV1& e = envRes.value;

    if (e.packId != pack.manifest.id) {
        return fail<CampaignState>(WotrErrc::IncompatiblePack,
                                   "save pack '" + e.packId +
                                       "' does not match campaign pack '" + pack.manifest.id + "'");
    }
    if (e.tacticalProfile != pack.manifest.tacticalProfile) {
        return fail<CampaignState>(WotrErrc::IncompatiblePack,
                                   "save tactical profile '" + e.tacticalProfile +
                                       "' does not match '" + pack.manifest.tacticalProfile + "'");
    }
    if (e.packHash != contentHashOf(pack)) {
        return fail<CampaignState>(WotrErrc::IncompatiblePack,
                                   "save pack hash does not match campaign pack content hash");
    }

    // Parse and validate the embedded state into a fresh instance; swap only
    // after every check passes (transactional load).
    JsonValue stateRoot;
    try {
        stateRoot = JsonValue::parse(e.stateBytes);
    } catch (const JsonParseError& ex) {
        return fail<CampaignState>(WotrErrc::InvalidSave, ex.what());
    }

    if (!stateRoot.isObject()) {
        return fail<CampaignState>(WotrErrc::InvalidSave,
                                   "embedded state is not an object");
    }

    CampaignState s;
    auto requireNode = [&](const JsonValue& obj, const std::string& key,
                           JsonValue::Type type) -> WotrResult<const JsonValue*> {
        const JsonValue* v = obj.get(key);
        if (v == nullptr || v->type != type) {
            return fail<const JsonValue*>(WotrErrc::InvalidSave,
                                          "embedded state: missing or wrong-typed field '" + key + "'");
        }
        return ok(v);
    };

    auto sv = requireNode(stateRoot, "schemaVersion", JsonValue::Type::UInt);
    if (!sv.ok()) return fail<CampaignState>(sv.err, sv.message);
    s.schemaVersion = static_cast<std::uint32_t>(sv.value->uintValue);
    if (s.schemaVersion != 1) {
        return fail<CampaignState>(WotrErrc::UnknownSaveVersion,
                                   "unsupported state schema version " +
                                       std::to_string(s.schemaVersion));
    }

    auto pid = requireNode(stateRoot, "packId", JsonValue::Type::String);
    if (!pid.ok()) return fail<CampaignState>(pid.err, pid.message);
    s.packId = pid.value->stringValue;
    if (s.packId != pack.manifest.id) {
        return fail<CampaignState>(WotrErrc::IncompatiblePack,
                                   "embedded state pack id mismatch");
    }

    auto ph = requireNode(stateRoot, "packHash", JsonValue::Type::String);
    if (!ph.ok()) return fail<CampaignState>(ph.err, ph.message);
    s.packHash = ph.value->stringValue;
    if (s.packHash != contentHashOf(pack)) {
        return fail<CampaignState>(WotrErrc::IncompatiblePack,
                                   "embedded state pack hash mismatch");
    }

    auto tp = requireNode(stateRoot, "tacticalProfile", JsonValue::Type::String);
    if (!tp.ok()) return fail<CampaignState>(tp.err, tp.message);
    s.tacticalProfile = tp.value->stringValue;

    auto tm = requireNode(stateRoot, "tacticalMap", JsonValue::Type::String);
    if (!tm.ok()) return fail<CampaignState>(tm.err, tm.message);
    s.tacticalMap = tm.value->stringValue;

    auto isd = requireNode(stateRoot, "initialSeed", JsonValue::Type::UInt);
    if (!isd.ok()) return fail<CampaignState>(isd.err, isd.message);
    s.initialSeed = isd.value->uintValue;

    auto turnNode = requireNode(stateRoot, "turn", JsonValue::Type::UInt);
    if (!turnNode.ok()) return fail<CampaignState>(turnNode.err, turnNode.message);
    s.turn = static_cast<std::uint32_t>(turnNode.value->uintValue);

    auto phaseNode = requireNode(stateRoot, "phase", JsonValue::Type::UInt);
    if (!phaseNode.ok()) return fail<CampaignState>(phaseNode.err, phaseNode.message);
    s.phase = static_cast<Phase>(phaseNode.value->uintValue);

    // factions
    auto facNode = requireNode(stateRoot, "factions", JsonValue::Type::Array);
    if (!facNode.ok()) return fail<CampaignState>(facNode.err, facNode.message);
    for (const auto& item : facNode.value->arrayValue) {
        if (!item.isObject()) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: factions[*] is not an object");
        }
        const JsonValue* idN = item.get("id");
        const JsonValue* nameN = item.get("name");
        if (!idN || idN->type != JsonValue::Type::String || !nameN ||
            nameN->type != JsonValue::Type::String) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: faction missing id or name");
        }
        StateFaction st;
        st.id = idN->stringValue;
        st.name = nameN->stringValue;
        if (!s.factions.emplace(st.id, std::move(st)).second) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: duplicate faction id '" + st.id + "'");
        }
    }

    // regions
    auto regNode = requireNode(stateRoot, "regions", JsonValue::Type::Array);
    if (!regNode.ok()) return fail<CampaignState>(regNode.err, regNode.message);
    for (const auto& item : regNode.value->arrayValue) {
        if (!item.isObject()) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: regions[*] is not an object");
        }
        const JsonValue* idN = item.get("id");
        const JsonValue* nameN = item.get("name");
        const JsonValue* ownerN = item.get("owner");
        const JsonValue* conN = item.get("connections");
        if (!idN || idN->type != JsonValue::Type::String || !nameN ||
            nameN->type != JsonValue::Type::String || !conN ||
            conN->type != JsonValue::Type::Array) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: region missing id, name or connections");
        }
        StateRegion st;
        st.id = idN->stringValue;
        st.name = nameN->stringValue;
        if (ownerN->type == JsonValue::Type::String) {
            st.owner = ownerN->stringValue;
        } else if (ownerN->type != JsonValue::Type::Null) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: region owner has wrong type");
        }
        for (const auto& c : conN->arrayValue) {
            if (c.type != JsonValue::Type::String) {
                return fail<CampaignState>(WotrErrc::InvalidSave,
                                           "embedded state: region connection is not a string");
            }
            st.connections.push_back(c.stringValue);
        }
        std::sort(st.connections.begin(), st.connections.end());
        if (!s.regions.emplace(st.id, std::move(st)).second) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: duplicate region id '" + st.id + "'");
        }
    }

    // armies
    auto armNode = requireNode(stateRoot, "armies", JsonValue::Type::Array);
    if (!armNode.ok()) return fail<CampaignState>(armNode.err, armNode.message);
    for (const auto& item : armNode.value->arrayValue) {
        if (!item.isObject()) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: armies[*] is not an object");
        }
        const JsonValue* idN = item.get("id");
        const JsonValue* fidN = item.get("factionId");
        const JsonValue* ridN = item.get("regionId");
        const JsonValue* memN = item.get("members");
        if (!idN || idN->type != JsonValue::Type::String || !fidN ||
            fidN->type != JsonValue::Type::String || !ridN ||
            ridN->type != JsonValue::Type::String || !memN ||
            memN->type != JsonValue::Type::Array) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: army missing id, factionId, regionId or members");
        }
        StateArmy st;
        st.id = idN->stringValue;
        st.factionId = fidN->stringValue;
        st.regionId = ridN->stringValue;
        std::set<MemberId> seenMembers;
        for (const auto& mem : memN->arrayValue) {
            if (!mem.isObject()) {
                return fail<CampaignState>(WotrErrc::InvalidSave,
                                           "embedded state: army member is not an object");
            }
            const JsonValue* midN = mem.get("id");
            const JsonValue* kindN = mem.get("kind");
            const JsonValue* qtyN = mem.get("quantity");
            const JsonValue* xpN = mem.get("experience");
            const JsonValue* upN = mem.get("persistentUpgrades");
            if (!midN || midN->type != JsonValue::Type::String || !kindN ||
                kindN->type != JsonValue::Type::String || !qtyN ||
                qtyN->type != JsonValue::Type::UInt || !xpN ||
                xpN->type != JsonValue::Type::UInt || !upN ||
                upN->type != JsonValue::Type::Array) {
                return fail<CampaignState>(WotrErrc::InvalidSave,
                                           "embedded state: army member has missing or wrong-typed fields");
            }
            StateMember sm;
            sm.id = midN->stringValue;
            sm.kind = kindN->stringValue;
            sm.quantity = qtyN->uintValue;
            sm.experience = xpN->uintValue;
            for (const auto& u : upN->arrayValue) {
                if (u.type != JsonValue::Type::String) {
                    return fail<CampaignState>(WotrErrc::InvalidSave,
                                               "embedded state: upgrade is not a string");
                }
                sm.persistentUpgrades.push_back(u.stringValue);
            }
            if (!seenMembers.insert(sm.id).second) {
                return fail<CampaignState>(WotrErrc::InvalidSave,
                                           "embedded state: duplicate member id '" + sm.id + "'");
            }
            st.members.push_back(sm);
        }
        std::sort(st.members.begin(), st.members.end(),
                  [](const StateMember& x, const StateMember& y) { return x.id < y.id; });

        // referential integrity
        if (s.factions.find(st.factionId) == s.factions.end()) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: army '" + st.id +
                                           "' references unknown faction '" + st.factionId + "'");
        }
        if (s.regions.find(st.regionId) == s.regions.end()) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: army '" + st.id +
                                           "' references unknown region '" + st.regionId + "'");
        }
        if (!s.armies.emplace(st.id, std::move(st)).second) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: duplicate army id '" + st.id + "'");
        }
    }

    // movedThisTurn
    auto mtNode = requireNode(stateRoot, "movedThisTurn", JsonValue::Type::Array);
    if (!mtNode.ok()) return fail<CampaignState>(mtNode.err, mtNode.message);
    for (const auto& item : mtNode.value->arrayValue) {
        if (item.type != JsonValue::Type::String) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: movedThisTurn[*] is not a string");
        }
        s.movedThisTurn.insert(item.stringValue);
    }

    // pendingBattle
    const JsonValue* pbN = stateRoot.get("pendingBattle");
    if (pbN != nullptr && pbN->type != JsonValue::Type::Null) {
        if (!pbN->isObject()) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: pendingBattle is not an object");
        }
        const JsonValue* bidN = pbN->get("id");
        const JsonValue* brN = pbN->get("regionId");
        const JsonValue* bpN = pbN->get("participants");
        const JsonValue* bchkN = pbN->get("requestChecksum");
        if (!bidN || bidN->type != JsonValue::Type::String || !brN ||
            brN->type != JsonValue::Type::String || !bpN ||
            bpN->type != JsonValue::Type::Array || !bchkN ||
            bchkN->type != JsonValue::Type::String) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: pendingBattle missing fields");
        }
        PendingBattle pb;
        pb.id = bidN->stringValue;
        pb.regionId = brN->stringValue;
        for (const auto& p : bpN->arrayValue) {
            if (p.type != JsonValue::Type::String) {
                return fail<CampaignState>(WotrErrc::InvalidSave,
                                           "embedded state: pendingBattle participant is not a string");
            }
            pb.participants.push_back(p.stringValue);
        }
        pb.requestChecksum = bchkN->stringValue;
        if (s.regions.find(pb.regionId) == s.regions.end()) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: pending battle references unknown region '" +
                                           pb.regionId + "'");
        }
        for (const auto& aid : pb.participants) {
            if (s.armies.find(aid) == s.armies.end()) {
                return fail<CampaignState>(WotrErrc::InvalidSave,
                                           "embedded state: pending battle references unknown army '" +
                                               aid + "'");
            }
        }
        s.pendingBattle = pb;
    }

    // acceptedCommands
    auto acNode = requireNode(stateRoot, "acceptedCommands", JsonValue::Type::Array);
    if (!acNode.ok()) return fail<CampaignState>(acNode.err, acNode.message);
    for (const auto& item : acNode.value->arrayValue) {
        if (!item.isObject()) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: accepted command is not an object");
        }
        CommandEnvelope cmd;
        const JsonValue* f;
        f = item.get("schemaVersion");
        if (!f || f->type != JsonValue::Type::UInt) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: command missing schemaVersion");
        }
        if (f->uintValue != 1) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: unsupported command schemaVersion " +
                                           std::to_string(f->uintValue));
        }
        cmd.schemaVersion = static_cast<std::uint32_t>(f->uintValue);
        f = item.get("campaignId");
        if (!f || f->type != JsonValue::Type::String) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: command missing campaignId");
        }
        cmd.campaignId = f->stringValue;
        f = item.get("turn");
        if (!f || f->type != JsonValue::Type::UInt) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: command missing turn");
        }
        cmd.turn = static_cast<std::uint32_t>(f->uintValue);
        f = item.get("playerId");
        if (!f || f->type != JsonValue::Type::String) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: command missing playerId");
        }
        cmd.playerId = f->stringValue;
        f = item.get("clientSequence");
        if (!f || f->type != JsonValue::Type::UInt) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: command missing clientSequence");
        }
        cmd.clientSequence = f->uintValue;
        f = item.get("commandId");
        if (!f || f->type != JsonValue::Type::String) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: command missing commandId");
        }
        cmd.commandId = f->stringValue;
        f = item.get("payload");
        if (!f || !f->isObject()) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: command missing payload");
        }
        const JsonValue* pt = f->get("type");
        const JsonValue* pm = f->get("move");
        if (!pt || pt->type != JsonValue::Type::UInt || !pm || !pm->isObject()) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: command payload missing type or move");
        }
        if (pt->uintValue != static_cast<std::uint64_t>(CommandType::MoveArmy)) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: unsupported command payload type " +
                                           std::to_string(pt->uintValue));
        }
        cmd.payload.type = CommandType::MoveArmy;
        const JsonValue* maid = pm->get("armyId");
        const JsonValue* mtid = pm->get("targetRegionId");
        if (!maid || maid->type != JsonValue::Type::String || !mtid ||
            mtid->type != JsonValue::Type::String) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: command move missing army ids");
        }
        cmd.payload.move.armyId = maid->stringValue;
        cmd.payload.move.targetRegionId = mtid->stringValue;
        f = item.get("checksum");
        if (!f || f->type != JsonValue::Type::String) {
            return fail<CampaignState>(WotrErrc::InvalidSave,
                                       "embedded state: command missing checksum");
        }
        cmd.checksum = f->stringValue;
        const std::string expectedCmdChecksum = envelopeChecksum(cmd);
        if (cmd.checksum != expectedCmdChecksum) {
            return fail<CampaignState>(
                WotrErrc::DamagedChecksum,
                "embedded state: accepted command '" + cmd.commandId +
                    "' checksum '" + cmd.checksum +
                    "' does not match recomputed envelopeChecksum '" +
                    expectedCmdChecksum + "'");
        }
        s.acceptedCommands.push_back(std::move(cmd));
    }

    const std::string parsedStateHash = stateHash(s);
    if (e.stateHash != parsedStateHash) {
        return fail<CampaignState>(
            WotrErrc::DamagedSaveChecksum,
            "save envelope stateHash '" + e.stateHash +
                "' does not match canonical state hash '" + parsedStateHash + "'");
    }
    if (canonicalState(s) != e.stateBytes) {
        return fail<CampaignState>(
            WotrErrc::DamagedSaveChecksum,
            "save envelope stateBytes do not match canonical state bytes");
    }

    return ok(s);
}

} // namespace WotrRemaster
