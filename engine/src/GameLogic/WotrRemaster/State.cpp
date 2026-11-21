// OpenBFME. GPL-3.0.

#include "State.h"

#include <algorithm>
#include <set>
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

JsonValue jbool(bool v) {
    JsonValue x;
    x.type = JsonValue::Type::Bool;
    x.boolean = v;
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

// Required member helper used when parsing state/save JSON.
WotrResult<const JsonValue*> requireNode(const JsonValue& obj, const std::string& key,
                                         JsonValue::Type type, const std::string& ctx) {
    const JsonValue* v = obj.get(key);
    if (v == nullptr) {
        return fail<const JsonValue*>(WotrErrc::InvalidSave,
                                      ctx + ": missing required field '" + key + "'");
    }
    if (v->type != type) {
        return fail<const JsonValue*>(WotrErrc::InvalidSave,
                                      ctx + ": field '" + key + "' has wrong type");
    }
    return ok(v);
}

} // namespace

std::string envelopeChecksum(const CommandEnvelope& e) {
    JsonValue env = jobject();
    env.objectValue.emplace("schemaVersion", juint(e.schemaVersion));
    env.objectValue.emplace("campaignId", jstr(e.campaignId));
    env.objectValue.emplace("turn", juint(e.turn));
    env.objectValue.emplace("playerId", jstr(e.playerId));
    env.objectValue.emplace("clientSequence", juint(e.clientSequence));
    env.objectValue.emplace("commandId", jstr(e.commandId));
    JsonValue payload = jobject();
    payload.objectValue.emplace("type", juint(static_cast<std::uint64_t>(e.payload.type)));
    JsonValue move = jobject();
    move.objectValue.emplace("armyId", jstr(e.payload.move.armyId));
    move.objectValue.emplace("targetRegionId", jstr(e.payload.move.targetRegionId));
    payload.objectValue.emplace("move", std::move(move));
    env.objectValue.emplace("payload", std::move(payload));
    return Sha256::hashHex(env.canonical());
}

std::string battleRequestChecksum(const BattleRequestV1& r) {
    BattleRequestV1 copy = r;
    copy.requestChecksum.clear();
    JsonValue req = jobject();
    req.objectValue.emplace("contractVersion", juint(r.contractVersion));
    req.objectValue.emplace("id", jstr(r.id));
    req.objectValue.emplace("stateHash", jstr(r.stateHash));
    req.objectValue.emplace("packId", jstr(r.packId));
    req.objectValue.emplace("packHash", jstr(r.packHash));
    req.objectValue.emplace("tacticalProfile", jstr(r.tacticalProfile));
    req.objectValue.emplace("tacticalMap", jstr(r.tacticalMap));
    req.objectValue.emplace("tacticalSeed", juint(r.tacticalSeed));
    req.objectValue.emplace("regionId", jstr(r.regionId));
    JsonValue participants = jarray();
    for (const auto& pa : r.participants) {
        JsonValue item = jobject();
        item.objectValue.emplace("id", jstr(pa.id));
        item.objectValue.emplace("factionId", jstr(pa.factionId));
        item.objectValue.emplace("side", juint(pa.side));
        JsonValue members = jarray();
        std::vector<TacticalMember> membersSorted = pa.members;
        std::sort(membersSorted.begin(), membersSorted.end(),
                  [](const TacticalMember& x, const TacticalMember& y) {
                      return x.id < y.id;
                  });
        for (const auto& m : membersSorted) {
            JsonValue mitem = jobject();
            mitem.objectValue.emplace("id", jstr(m.id));
            mitem.objectValue.emplace("templateId", jstr(m.templateId));
            mitem.objectValue.emplace("quantity", juint(m.quantity));
            mitem.objectValue.emplace("experience", juint(m.experience));
            JsonValue ups = jarray();
            for (const auto& u : m.persistentUpgrades) {
                JsonValue uv; uv.type = JsonValue::Type::String; uv.stringValue = u;
                ups.arrayValue.push_back(std::move(uv));
            }
            mitem.objectValue.emplace("persistentUpgrades", std::move(ups));
            members.arrayValue.push_back(std::move(mitem));
        }
        item.objectValue.emplace("members", std::move(members));
        participants.arrayValue.push_back(std::move(item));
    }
    req.objectValue.emplace("participants", std::move(participants));
    JsonValue seats = jarray();
    for (const auto& seat : r.seats) {
        JsonValue item = jobject();
        item.objectValue.emplace("playerId", jstr(seat.playerId));
        item.objectValue.emplace("human", jbool(seat.human));
        seats.arrayValue.push_back(std::move(item));
    }
    req.objectValue.emplace("seats", std::move(seats));
    req.objectValue.emplace("startingResources", juint(r.startingResources));
    JsonValue sciences = jarray();
    for (const auto& s : r.persistentSciences) {
        JsonValue sv; sv.type = JsonValue::Type::String; sv.stringValue = s;
        sciences.arrayValue.push_back(std::move(sv));
    }
    req.objectValue.emplace("persistentSciences", std::move(sciences));
    JsonValue reinforcements = jarray();
    for (const auto& s : r.reinforcements) {
        JsonValue sv; sv.type = JsonValue::Type::String; sv.stringValue = s;
        reinforcements.arrayValue.push_back(std::move(sv));
    }
    req.objectValue.emplace("reinforcements", std::move(reinforcements));
    JsonValue objectives = jarray();
    for (const auto& o : r.strategicObjectives) {
        JsonValue ov; ov.type = JsonValue::Type::String; ov.stringValue = o;
        objectives.arrayValue.push_back(std::move(ov));
    }
    req.objectValue.emplace("strategicObjectives", std::move(objectives));
    JsonValue heroes = jarray();
    for (const auto& h : r.heroes) {
        JsonValue hv; hv.type = JsonValue::Type::String; hv.stringValue = h;
        heroes.arrayValue.push_back(std::move(hv));
    }
    req.objectValue.emplace("heroes", std::move(heroes));
    return Sha256::hashHex(req.canonical());
}

std::string battleResultChecksum(const BattleResultV1& r) {
    JsonValue res = jobject();
    res.objectValue.emplace("contractVersion", juint(r.contractVersion));
    res.objectValue.emplace("id", jstr(r.id));
    res.objectValue.emplace("requestChecksum", jstr(r.requestChecksum));
    res.objectValue.emplace("completionReason", juint(static_cast<std::uint64_t>(r.completionReason)));
    if (r.victor) res.objectValue.emplace("victor", jstr(*r.victor));
    else res.objectValue.emplace("victor", jnull());
    JsonValue survivors = jarray();
    std::vector<SurvivingArmy> survivorsSorted = r.survivors;
    std::sort(survivorsSorted.begin(), survivorsSorted.end(),
              [](const SurvivingArmy& x, const SurvivingArmy& y) {
                  return x.id < y.id;
              });
    for (const auto& sa : survivorsSorted) {
        JsonValue item = jobject();
        item.objectValue.emplace("id", jstr(sa.id));
        JsonValue members = jarray();
        std::vector<SurvivingMember> membersSorted = sa.members;
        std::sort(membersSorted.begin(), membersSorted.end(),
                  [](const SurvivingMember& x, const SurvivingMember& y) {
                      return x.id < y.id;
                  });
        for (const auto& m : membersSorted) {
            JsonValue mitem = jobject();
            mitem.objectValue.emplace("id", jstr(m.id));
            mitem.objectValue.emplace("quantity", juint(m.quantity));
            mitem.objectValue.emplace("experience", juint(m.experience));
            JsonValue ups = jarray();
            for (const auto& u : m.persistentUpgrades) {
                JsonValue uv; uv.type = JsonValue::Type::String; uv.stringValue = u;
                ups.arrayValue.push_back(std::move(uv));
            }
            mitem.objectValue.emplace("persistentUpgrades", std::move(ups));
            members.arrayValue.push_back(std::move(mitem));
        }
        item.objectValue.emplace("members", std::move(members));
        survivors.arrayValue.push_back(std::move(item));
    }
    res.objectValue.emplace("survivors", std::move(survivors));
    JsonValue heroes = jarray();
    for (const auto& h : r.heroes) {
        JsonValue item = jobject();
        item.objectValue.emplace("id", jstr(h.id));
        item.objectValue.emplace("survived", jbool(h.survived));
        item.objectValue.emplace("revived", jbool(h.revived));
        heroes.arrayValue.push_back(std::move(item));
    }
    res.objectValue.emplace("heroes", std::move(heroes));
    JsonValue sciences = jarray();
    for (const auto& s : r.persistentSciences) {
        JsonValue sv; sv.type = JsonValue::Type::String; sv.stringValue = s;
        sciences.arrayValue.push_back(std::move(sv));
    }
    res.objectValue.emplace("persistentSciences", std::move(sciences));
    JsonValue called = jarray();
    for (const auto& a : r.reinforcements.called) {
        JsonValue av; av.type = JsonValue::Type::String; av.stringValue = a;
        called.arrayValue.push_back(std::move(av));
    }
    JsonValue uncalled = jarray();
    for (const auto& a : r.reinforcements.uncalled) {
        JsonValue av; av.type = JsonValue::Type::String; av.stringValue = a;
        uncalled.arrayValue.push_back(std::move(av));
    }
    JsonValue rein = jobject();
    rein.objectValue.emplace("called", std::move(called));
    rein.objectValue.emplace("uncalled", std::move(uncalled));
    res.objectValue.emplace("reinforcements", std::move(rein));
    JsonValue destroyed = jarray();
    for (const auto& o : r.destroyedObjectives) {
        JsonValue ov; ov.type = JsonValue::Type::String; ov.stringValue = o;
        destroyed.arrayValue.push_back(std::move(ov));
    }
    res.objectValue.emplace("destroyedObjectives", std::move(destroyed));
    res.objectValue.emplace("tacticalStatistics", jstr(r.tacticalStatistics));
    if (r.captureFrame) res.objectValue.emplace("captureFrame", juint(*r.captureFrame));
    else res.objectValue.emplace("captureFrame", jnull());
    return Sha256::hashHex(res.canonical());
}

JsonValue stateToJson(const CampaignState& s) {
    JsonValue root = jobject();
    root.objectValue.emplace("schemaVersion", juint(s.schemaVersion));
    root.objectValue.emplace("packId", jstr(s.packId));
    root.objectValue.emplace("packHash", jstr(s.packHash));
    root.objectValue.emplace("tacticalProfile", jstr(s.tacticalProfile));
    root.objectValue.emplace("tacticalMap", jstr(s.tacticalMap));
    root.objectValue.emplace("initialSeed", juint(s.initialSeed));
    root.objectValue.emplace("turn", juint(s.turn));
    root.objectValue.emplace("phase", juint(static_cast<std::uint64_t>(s.phase)));

    JsonValue factions = jarray();
    for (const auto& kv : s.factions) {
        JsonValue item = jobject();
        item.objectValue.emplace("id", jstr(kv.first));
        item.objectValue.emplace("name", jstr(kv.second.name));
        factions.arrayValue.push_back(std::move(item));
    }
    root.objectValue.emplace("factions", std::move(factions));

    JsonValue regions = jarray();
    for (const auto& kv : s.regions) {
        const StateRegion& r = kv.second;
        JsonValue item = jobject();
        item.objectValue.emplace("id", jstr(r.id));
        item.objectValue.emplace("name", jstr(r.name));
        if (r.owner) item.objectValue.emplace("owner", jstr(*r.owner));
        else item.objectValue.emplace("owner", jnull());
        JsonValue con = jarray();
        std::vector<std::string> conSorted = r.connections;
        std::sort(conSorted.begin(), conSorted.end());
        for (const auto& c : conSorted) {
            JsonValue cv; cv.type = JsonValue::Type::String; cv.stringValue = c;
            con.arrayValue.push_back(std::move(cv));
        }
        item.objectValue.emplace("connections", std::move(con));
        regions.arrayValue.push_back(std::move(item));
    }
    root.objectValue.emplace("regions", std::move(regions));

    JsonValue armies = jarray();
    for (const auto& kv : s.armies) {
        const StateArmy& a = kv.second;
        JsonValue item = jobject();
        item.objectValue.emplace("id", jstr(a.id));
        item.objectValue.emplace("factionId", jstr(a.factionId));
        item.objectValue.emplace("regionId", jstr(a.regionId));
        JsonValue members = jarray();
        std::vector<StateMember> membersSorted = a.members;
        std::sort(membersSorted.begin(), membersSorted.end(),
                  [](const StateMember& x, const StateMember& y) {
                      return x.id < y.id;
                  });
        for (const auto& m : membersSorted) {
            JsonValue mitem = jobject();
            mitem.objectValue.emplace("id", jstr(m.id));
            mitem.objectValue.emplace("kind", jstr(m.kind));
            mitem.objectValue.emplace("quantity", juint(m.quantity));
            mitem.objectValue.emplace("experience", juint(m.experience));
            JsonValue ups = jarray();
            std::vector<UpgradeId> upsSorted = m.persistentUpgrades;
            std::sort(upsSorted.begin(), upsSorted.end());
            for (const auto& u : upsSorted) {
                JsonValue uv; uv.type = JsonValue::Type::String; uv.stringValue = u;
                ups.arrayValue.push_back(std::move(uv));
            }
            mitem.objectValue.emplace("persistentUpgrades", std::move(ups));
            members.arrayValue.push_back(std::move(mitem));
        }
        item.objectValue.emplace("members", std::move(members));
        armies.arrayValue.push_back(std::move(item));
    }
    root.objectValue.emplace("armies", std::move(armies));

    JsonValue moved = jarray();
    for (const auto& aid : s.movedThisTurn) {
        JsonValue av; av.type = JsonValue::Type::String; av.stringValue = aid;
        moved.arrayValue.push_back(std::move(av));
    }
    root.objectValue.emplace("movedThisTurn", std::move(moved));

    if (s.pendingBattle) {
        JsonValue pb = jobject();
        pb.objectValue.emplace("id", jstr(s.pendingBattle->id));
        pb.objectValue.emplace("regionId", jstr(s.pendingBattle->regionId));
        JsonValue parts = jarray();
        for (const auto& aid : s.pendingBattle->participants) {
            JsonValue pv; pv.type = JsonValue::Type::String; pv.stringValue = aid;
            parts.arrayValue.push_back(std::move(pv));
        }
        pb.objectValue.emplace("participants", std::move(parts));
        pb.objectValue.emplace("requestChecksum", jstr(s.pendingBattle->requestChecksum));
        root.objectValue.emplace("pendingBattle", std::move(pb));
    } else {
        root.objectValue.emplace("pendingBattle", jnull());
    }

    JsonValue accepted = jarray();
    for (const auto& e : s.acceptedCommands) {
        JsonValue item = jobject();
        item.objectValue.emplace("schemaVersion", juint(e.schemaVersion));
        item.objectValue.emplace("campaignId", jstr(e.campaignId));
        item.objectValue.emplace("turn", juint(e.turn));
        item.objectValue.emplace("playerId", jstr(e.playerId));
        item.objectValue.emplace("clientSequence", juint(e.clientSequence));
        item.objectValue.emplace("commandId", jstr(e.commandId));
        JsonValue payload = jobject();
        payload.objectValue.emplace("type", juint(static_cast<std::uint64_t>(e.payload.type)));
        JsonValue move = jobject();
        move.objectValue.emplace("armyId", jstr(e.payload.move.armyId));
        move.objectValue.emplace("targetRegionId", jstr(e.payload.move.targetRegionId));
        payload.objectValue.emplace("move", std::move(move));
        item.objectValue.emplace("payload", std::move(payload));
        item.objectValue.emplace("checksum", jstr(e.checksum));
        accepted.arrayValue.push_back(std::move(item));
    }
    root.objectValue.emplace("acceptedCommands", std::move(accepted));

    return root;
}

std::string canonicalState(const CampaignState& s) {
    return stateToJson(s).canonical();
}

std::string stateHash(const CampaignState& s) {
    return Sha256::hashHex(canonicalState(s));
}

WotrResult<CampaignState> initialState(const Pack& pack, std::uint64_t seed) {
    CampaignState s;
    s.schemaVersion = 1;
    s.packId = pack.manifest.id;
    s.packHash = contentHashOf(pack);
    s.tacticalProfile = pack.manifest.tacticalProfile;
    s.tacticalMap = pack.manifest.tacticalMap;
    s.initialSeed = seed;

    for (const auto& f : pack.factions) {
        StateFaction st;
        st.id = f.id;
        st.name = f.name;
        s.factions.emplace(f.id, std::move(st));
    }
    for (const auto& r : pack.regions) {
        StateRegion st;
        st.id = r.id;
        st.name = r.name;
        st.owner = std::nullopt;
        st.connections = r.connections;
        s.regions.emplace(r.id, std::move(st));
    }
    for (const auto& a : pack.armies) {
        StateArmy st;
        st.id = a.id;
        st.factionId = a.factionId;
        st.regionId = a.regionId;
        for (const auto& m : a.members) {
            StateMember sm;
            sm.id = m.id;
            sm.kind = m.kind;
            sm.quantity = m.quantity;
            sm.experience = m.experience;
            sm.persistentUpgrades = m.persistentUpgrades;
            st.members.push_back(sm);
        }
        s.armies.emplace(a.id, std::move(st));
    }
    return ok(s);
}

WotrResult<ResolutionOutcome> resolveCommands(CampaignState state,
                                              std::vector<CommandEnvelope> envelopes) {
    ResolutionOutcome out;
    out.state = state;

    // ---- 1. Validate every envelope before mutating anything --------------
    std::set<ArmyId> movedCheck = state.movedThisTurn;
    for (const auto& e : envelopes) {
        if (e.schemaVersion != 1) {
            return fail<ResolutionOutcome>(WotrErrc::InvalidCommand,
                                           "command " + e.commandId +
                                               ": unsupported envelope schema version " +
                                               std::to_string(e.schemaVersion));
        }
        if (e.campaignId != state.packId) {
            return fail<ResolutionOutcome>(WotrErrc::InvalidCommand,
                                           "command " + e.commandId +
                                               ": campaign " + e.campaignId +
                                               " does not match pack " + state.packId);
        }
        if (e.turn != state.turn) {
            return fail<ResolutionOutcome>(WotrErrc::InvalidCommand,
                                           "command " + e.commandId +
                                               ": turn " + std::to_string(e.turn) +
                                               " does not match state turn " +
                                               std::to_string(state.turn));
        }
        if (envelopeChecksum(e) != e.checksum) {
            return fail<ResolutionOutcome>(WotrErrc::DamagedChecksum,
                                           "command " + e.commandId + ": damaged envelope checksum");
        }
        auto ait = state.armies.find(e.payload.move.armyId);
        if (ait == state.armies.end()) {
            return fail<ResolutionOutcome>(WotrErrc::UnknownEntity,
                                           "command " + e.commandId + ": unknown army " +
                                               e.payload.move.armyId);
        }
        if (e.playerId != ait->second.factionId) {
            return fail<ResolutionOutcome>(WotrErrc::InvalidCommand,
                                           "command " + e.commandId + ": player " + e.playerId +
                                               " does not own army " + e.payload.move.armyId);
        }
        const StateRegion& reg = state.regions.at(ait->second.regionId);
        if (std::find(reg.connections.begin(), reg.connections.end(),
                      e.payload.move.targetRegionId) == reg.connections.end()) {
            return fail<ResolutionOutcome>(WotrErrc::InvalidCommand,
                                           "command " + e.commandId + ": army " +
                                               e.payload.move.armyId +
                                               " cannot move to non-adjacent region " +
                                               e.payload.move.targetRegionId);
        }
        if (!movedCheck.insert(e.payload.move.armyId).second) {
            return fail<ResolutionOutcome>(WotrErrc::InvalidCommand,
                                           "command " + e.commandId + ": army " +
                                               e.payload.move.armyId +
                                               " already moved this turn");
        }
    }

    // ---- 2. Canonical resolution order ----------------------------------
    std::sort(envelopes.begin(), envelopes.end(),
              [](const CommandEnvelope& a, const CommandEnvelope& b) {
                  if (a.playerId != b.playerId) return a.playerId < b.playerId;
                  if (a.clientSequence != b.clientSequence) return a.clientSequence < b.clientSequence;
                  return a.commandId < b.commandId;
              });

    // ---- 3. Apply moves ---------------------------------------------------
    for (const auto& e : envelopes) {
        StateArmy& army = out.state.armies.at(e.payload.move.armyId);
        army.regionId = e.payload.move.targetRegionId;
        out.state.movedThisTurn.insert(e.payload.move.armyId);
        out.state.acceptedCommands.push_back(e);
        out.events.push_back({ResolutionEventType::ArmyMoved, e.payload.move.armyId,
                              e.payload.move.targetRegionId, ""});
    }

    // ---- 4. Conflict detection: exactly one pending battle per round -----
    std::map<RegionId, std::vector<ArmyId>> byRegion;
    for (const auto& kv : out.state.armies) {
        byRegion[kv.second.regionId].push_back(kv.first);
    }
    for (const auto& kv : byRegion) {
        const RegionId& regionId = kv.first;
        const std::vector<ArmyId>& ids = kv.second;
        std::set<FactionId> sides;
        for (const auto& aid : ids) sides.insert(out.state.armies.at(aid).factionId);
        if (sides.size() <= 1) continue;

        BattleId battleId = "battle:" + regionId + ":" + std::to_string(state.turn);

        // Side assignment: deterministic, by sorted faction order.
        std::map<FactionId, std::uint8_t> sideByFaction;
        std::uint8_t nextSide = 0;
        for (const auto& aid : ids) {
            sideByFaction.emplace(out.state.armies.at(aid).factionId, nextSide++);
        }

        BattleRequestV1 req;
        req.contractVersion = 1;
        req.id = battleId;
        req.stateHash = stateHash(out.state);
        req.packId = state.packId;
        req.packHash = state.packHash;
        req.tacticalProfile = state.tacticalProfile;
        req.tacticalMap = state.tacticalMap;
        req.tacticalSeed = state.initialSeed;
        req.regionId = regionId;
        for (const auto& aid : ids) {
            const StateArmy& army = out.state.armies.at(aid);
            TacticalArmy ta;
            ta.id = aid;
            ta.factionId = army.factionId;
            ta.side = sideByFaction[army.factionId];
            for (const auto& m : army.members) {
                TacticalMember tm;
                tm.id = m.id;
                tm.templateId = m.kind;
                tm.quantity = m.quantity;
                tm.experience = m.experience;
                tm.persistentUpgrades = m.persistentUpgrades;
                ta.members.push_back(tm);
            }
            req.participants.push_back(ta);
        }
        for (const auto& aid : ids) {
            TacticalSeat seat;
            seat.playerId = out.state.armies.at(aid).factionId;
            seat.human = true;
            req.seats.push_back(seat);
        }
        req.requestChecksum = battleRequestChecksum(req);

        PendingBattle pb;
        pb.id = battleId;
        pb.regionId = regionId;
        pb.participants = ids;  // already sorted by army id
        pb.requestChecksum = req.requestChecksum;

        out.request = req;
        out.state.pendingBattle = pb;
        out.events.push_back(
            {ResolutionEventType::BattlePending, "", regionId, battleId});
        out.state.phase = Phase::BattlePending;
        out.battlePending = true;
        break; // MVP: one pending battle per resolution round
    }

    return ok(out);
}

WotrResult<CampaignState> applyBattleResult(CampaignState state,
                                            const BattleResultV1& result) {
    if (!state.pendingBattle) {
        return fail<CampaignState>(WotrErrc::UnknownBattle,
                                   "no pending battle to complete");
    }
    if (result.id != state.pendingBattle->id) {
        return fail<CampaignState>(WotrErrc::UnknownBattle,
                                   "battle id '" + result.id +
                                       "' does not match the pending battle '" +
                                       state.pendingBattle->id + "'");
    }
    if (result.contractVersion != 1) {
        return fail<CampaignState>(WotrErrc::InvalidResult,
                                   "unsupported result contract version " +
                                       std::to_string(result.contractVersion));
    }
    if (result.requestChecksum != state.pendingBattle->requestChecksum) {
        return fail<CampaignState>(WotrErrc::MismatchedChecksum,
                                   "result requestChecksum does not match the pending battle request");
    }
    if (result.resultChecksum != battleResultChecksum(result)) {
        return fail<CampaignState>(WotrErrc::DamagedChecksum,
                                   "damaged battle result checksum");
    }

    // Validate survivor references before mutating anything.
    for (const auto& sa : result.survivors) {
        auto ait = state.armies.find(sa.id);
        if (ait == state.armies.end()) {
            return fail<CampaignState>(WotrErrc::UnknownEntity,
                                       "battle result: unknown army '" + sa.id + "'");
        }
        std::set<MemberId> known;
        for (const auto& m : ait->second.members) known.insert(m.id);
        for (const auto& sm : sa.members) {
            if (!known.count(sm.id)) {
                return fail<CampaignState>(WotrErrc::UnknownEntity,
                                           "battle result: unknown member '" + sm.id +
                                               "' in army '" + sa.id + "'");
            }
        }
    }

    // Apply: survivors replace their members; armies not in the result are
    // removed (casualties). Armies not in the battle are untouched.
    std::map<ArmyId, StateArmy> newArmies;
    for (auto& kv : state.armies) {
        const ArmyId& aid = kv.first;
        bool inBattle = false;
        for (const auto& p : state.pendingBattle->participants) {
            if (p == aid) { inBattle = true; break; }
        }
        if (!inBattle) {
            newArmies.emplace(aid, kv.second);
            continue;
        }
        const SurvivingArmy* surv = nullptr;
        for (const auto& s : result.survivors) {
            if (s.id == aid) { surv = &s; break; }
        }
        if (surv == nullptr) continue; // destroyed
        StateArmy na = kv.second;
        na.members.clear();
        for (const auto& sm : surv->members) {
            bool found = false;
            for (const auto& m : kv.second.members) {
                if (m.id == sm.id) {
                    StateMember nm;
                    nm.id = sm.id;
                    nm.kind = m.kind;
                    nm.persistentUpgrades = m.persistentUpgrades;
                    nm.quantity = sm.quantity;
                    nm.experience = sm.experience;
                    na.members.push_back(nm);
                    found = true;
                    break;
                }
            }
            if (!found) {
                return fail<CampaignState>(WotrErrc::UnknownEntity,
                                           "battle result: member '" + sm.id +
                                               "' not found in army '" + aid + "'");
            }
        }
        newArmies.emplace(aid, std::move(na));
    }
    state.armies = std::move(newArmies);

    // Region ownership passes to the victor; draw/forfeit leaves it contested.
    auto rit = state.regions.find(state.pendingBattle->regionId);
    if (rit != state.regions.end()) {
        rit->second.owner = result.victor;
    }

    state.pendingBattle = std::nullopt;
    state.phase = Phase::Planning;
    state.turn += 1;
    state.movedThisTurn.clear();
    return ok(state);
}

} // namespace WotrRemaster
