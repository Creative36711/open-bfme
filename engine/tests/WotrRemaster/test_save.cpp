// OpenBFME. GPL-3.0.
// WotrRemaster deterministic strategic core: save/load tests.

#include <doctest.h>

#include "test_common.h"

#include "GameLogic/WotrRemaster/Pack.h"
#include "GameLogic/WotrRemaster/Save.h"
#include "GameLogic/WotrRemaster/State.h"

using namespace WotrRemaster;
using namespace WotrRemasterTest;

namespace {

CommandEnvelope moveEnvelopeTest(std::uint32_t turn, const std::string& player,
                                 std::uint64_t seq, const std::string& commandId,
                                 const std::string& armyId,
                                 const std::string& targetRegion);

std::pair<Pack, CampaignState> resolvedState() {
    auto packRes = loadPack(readFile(fixtureDir() + "/pack.json"));
    INFO(packRes.message);
    REQUIRE(packRes.ok());
    Pack pack = packRes.value;
    auto st = initialState(pack, 42u);
    INFO(st.message);
    REQUIRE(st.ok());
    CampaignState s = st.value;

    CommandEnvelope g = moveEnvelopeTest(0, "faction_gondor", 1, "cmd-g-1",
                                         "army_gondor", "crossing");
    CommandEnvelope m = moveEnvelopeTest(0, "faction_mordor", 1, "cmd-m-1",
                                         "army_mordor", "crossing");
    auto res = resolveCommands(s, {g, m});
    REQUIRE(res.ok());
    REQUIRE(res.value.battlePending);

    BattleResultV1 result;
    result.contractVersion = 1;
    result.id = res.value.state.pendingBattle->id;
    result.requestChecksum = res.value.state.pendingBattle->requestChecksum;
    result.completionReason = CompletionReason::Victory;
    result.victor = "faction_gondor";
    SurvivingArmy sa;
    sa.id = "army_gondor";
    SurvivingMember sm;
    sm.id = "member_gondor_1";
    sm.quantity = 12;
    sm.experience = 3;
    sa.members.push_back(sm);
    result.survivors.push_back(sa);
    result.resultChecksum = battleResultChecksum(result);

    auto applied = applyBattleResult(res.value.state, result);
    REQUIRE(applied.ok());
    return {pack, applied.value};
}

CommandEnvelope moveEnvelopeTest(std::uint32_t turn, const std::string& player,
                                 std::uint64_t seq, const std::string& commandId,
                                 const std::string& armyId,
                                 const std::string& targetRegion) {
    CommandEnvelope e;
    e.schemaVersion = 1;
    e.campaignId = "wotr_1_1_tiny";
    e.turn = turn;
    e.playerId = player;
    e.clientSequence = seq;
    e.commandId = commandId;
    e.payload.type = CommandType::MoveArmy;
    e.payload.move.armyId = armyId;
    e.payload.move.targetRegionId = targetRegion;
    e.checksum = envelopeChecksum(e);
    return e;
}

} // namespace

TEST_CASE("save: save then load in a fresh instance yields equal bytes and hash") {
    auto [pack, s] = resolvedState();
    std::string envelope = saveCampaign(s, pack);
    CHECK_FALSE(envelope.empty());

    auto loaded = loadCampaign(envelope, pack);
    INFO(loaded.message);
    REQUIRE(loaded.ok());
    CHECK(canonicalState(loaded.value) == canonicalState(s));
    CHECK(stateHash(loaded.value) == stateHash(s));
    CHECK(loaded.value.turn == s.turn);
    CHECK(loaded.value.phase == s.phase);
    CHECK(loaded.value.regions.at("crossing").owner == "faction_gondor");
}

TEST_CASE("save: loaded instance continues with equal next-step hash") {
    auto [pack, s] = resolvedState(); // turn == 1, planning, nothing moved this turn

    std::string envelope = saveCampaign(s, pack);
    auto loaded = loadCampaign(envelope, pack);
    REQUIRE(loaded.ok());
    CHECK(stateHash(loaded.value) == stateHash(s));
    CHECK(canonicalState(loaded.value) == canonicalState(s));

    // Same next command on both sides -> equal next-step hash.
    // Both sides are post-battle: army_gondor in crossing, nothing moved this turn.
    CommandEnvelope again = moveEnvelopeTest(1, "faction_gondor", 1, "cmd-again-1",
                                             "army_gondor", "west");
    auto ra = resolveCommands(loaded.value, {again});
    auto rb = resolveCommands(s, {again});
    REQUIRE(ra.ok());
    REQUIRE(rb.ok());
    CHECK(stateHash(ra.value.state) == stateHash(rb.value.state));
    CHECK(canonicalState(ra.value.state) == canonicalState(rb.value.state));
}

TEST_CASE("save: damaged checksum is a named nonzero failure") {
    auto [pack, s] = resolvedState();
    std::string envelope = saveCampaign(s, pack);
    REQUIRE(envelope.size() > 100);
    // Flip a byte inside the stateBytes region.
    envelope[envelope.size() / 2] ^= 0xff;
    auto loaded = loadCampaign(envelope, pack);
    CHECK_FALSE(loaded.ok());
    CHECK(loaded.err == WotrErrc::DamagedSaveChecksum);
    CHECK(!loaded.message.empty());
}

TEST_CASE("save: unknown envelope version is a named nonzero failure") {
    auto [pack, s] = resolvedState();
    JsonValue root = JsonValue::parse(saveCampaign(s, pack));
    root.objectValue.at("envelopeVersion").uintValue = 2;
    auto loaded = loadCampaign(root.canonical(), pack);
    CHECK_FALSE(loaded.ok());
    CHECK(loaded.err == WotrErrc::UnknownSaveVersion);
    CHECK(loaded.message.find("version") != std::string::npos);
}

TEST_CASE("save: unknown contract version is a named nonzero failure") {
    auto [pack, s] = resolvedState();
    JsonValue root = JsonValue::parse(saveCampaign(s, pack));
    root.objectValue.at("contractVersion").uintValue = 9;
    auto loaded = loadCampaign(root.canonical(), pack);
    CHECK_FALSE(loaded.ok());
    CHECK(loaded.err == WotrErrc::UnknownSaveVersion);
}

TEST_CASE("save: incompatible pack is rejected") {
    auto [pack, s] = resolvedState();
    std::string envelope = saveCampaign(s, pack);
    // Same pack shape, different manifest id -> packId mismatch.
    Pack other = pack;
    other.manifest.id = "other_pack";
    auto loaded = loadCampaign(envelope, other);
    CHECK_FALSE(loaded.ok());
    CHECK(loaded.err == WotrErrc::IncompatiblePack);
}

TEST_CASE("save: incompatible tactical profile is rejected") {
    auto [pack, s] = resolvedState();
    std::string envelope = saveCampaign(s, pack);
    // Same pack id, different tactical profile -> profile mismatch.
    Pack other = pack;
    other.manifest.tacticalProfile = "some_other_profile";
    auto loaded = loadCampaign(envelope, other);
    CHECK_FALSE(loaded.ok());
    CHECK(loaded.err == WotrErrc::IncompatiblePack);
}

TEST_CASE("save: legitimate save envelope carries canonical stateHash and round-trips byte-identically") {
    auto [pack, s] = resolvedState();
    const std::string envelope = saveCampaign(s, pack);
    auto parsed = parseSaveEnvelope(envelope);
    INFO(parsed.message);
    REQUIRE(parsed.ok());
    CHECK(parsed.value.stateHash == stateHash(s));
    CHECK(parsed.value.stateBytes == canonicalState(s));
    CHECK(serializeSaveEnvelope(parsed.value) == envelope);

    auto loaded = loadCampaign(envelope, pack);
    INFO(loaded.message);
    REQUIRE(loaded.ok());
    CHECK(canonicalState(loaded.value) == canonicalState(s));
    CHECK(stateHash(loaded.value) == stateHash(s));
    CHECK(saveCampaign(loaded.value, pack) == envelope);
}

TEST_CASE("save: A1 PROBE 2 reproducer (re-sealed envelope with army_gondor.regionId = east) is refused") {
    auto packRes = loadPack(readFile(fixtureDir() + "/pack.json"));
    INFO(packRes.message);
    REQUIRE(packRes.ok());
    const Pack pack = packRes.value;

    auto st = initialState(pack, 7ull);
    INFO(st.message);
    REQUIRE(st.ok());
    auto r = resolveCommands(
        st.value,
        {moveEnvelopeTest(0, "faction_gondor", 1, "c1", "army_gondor", "crossing"),
         moveEnvelopeTest(0, "faction_mordor", 1, "c2", "army_mordor", "crossing")});
    INFO(r.message);
    REQUIRE(r.ok());
    REQUIRE(r.value.state.armies.at("army_gondor").regionId == "crossing");

    const std::string beforeBytes = canonicalState(r.value.state);
    const std::string beforeHash = stateHash(r.value.state);

    const std::string envelope = saveCampaign(r.value.state, pack);
    auto parsed = parseSaveEnvelope(envelope);
    INFO(parsed.message);
    REQUIRE(parsed.ok());
    CHECK(parsed.value.stateHash == beforeHash);

    // Re-seal the envelope over an altered inner state (army_gondor.regionId = east
    // while the army is in crossing), matching A1 PROBE 2 verbatim.
    WotrSaveEnvelopeV1 forged = parsed.value;
    JsonValue inner = JsonValue::parse(forged.stateBytes);
    for (auto& a : inner.objectValue.at("armies").arrayValue) {
        if (a.objectValue.at("id").stringValue == "army_gondor") {
            a.objectValue.at("regionId").stringValue = "east";
        }
    }
    forged.stateBytes = inner.canonical();
    forged.checksum = Sha256::hashHex(saveEnvelopePayload(forged));
    const std::string forgedEnvelope = serializeSaveEnvelope(forged);

    // Outer envelope checksum is validly re-sealed over the payload including stateHash.
    auto forgedParsed = parseSaveEnvelope(forgedEnvelope);
    REQUIRE(forgedParsed.ok());

    // loadCampaign cross-checks stateHash == stateHash(parsedState) and refuses.
    auto res = loadCampaign(forgedEnvelope, pack);
    CHECK_FALSE(res.ok());
    CHECK(res.err == WotrErrc::DamagedSaveChecksum);
    CHECK(std::string(errcName(res.err)) == "ERR_DAMAGED_SAVE_CHECKSUM");
    CHECK_FALSE(res.message.empty());
    CHECK(canonicalState(r.value.state) == beforeBytes);
    CHECK(stateHash(r.value.state) == beforeHash);

    // Legitimate envelope on the same state still loads byte-identically.
    auto legit = loadCampaign(envelope, pack);
    INFO(legit.message);
    REQUIRE(legit.ok());
    CHECK(canonicalState(legit.value) == beforeBytes);
    CHECK(stateHash(legit.value) == beforeHash);
}

TEST_CASE("save: stateHash is hashed inside envelope payload and cross-checked at load") {
    auto [pack, s] = resolvedState();
    const std::string envelope = saveCampaign(s, pack);
    auto parsed = parseSaveEnvelope(envelope);
    REQUIRE(parsed.ok());

    // Tampering with stateHash without re-sealing breaks the outer envelope checksum.
    JsonValue unsealed = JsonValue::parse(envelope);
    unsealed.objectValue.at("stateHash").stringValue = std::string(64, '0');
    auto badOuter = parseSaveEnvelope(unsealed.canonical());
    CHECK_FALSE(badOuter.ok());
    CHECK(badOuter.err == WotrErrc::DamagedSaveChecksum);

    // Re-sealing with a wrong stateHash over valid stateBytes fails at loadCampaign.
    WotrSaveEnvelopeV1 badHashEnv = parsed.value;
    badHashEnv.stateHash = std::string(64, '0');
    badHashEnv.checksum = Sha256::hashHex(saveEnvelopePayload(badHashEnv));
    const std::string resealedBadHash = serializeSaveEnvelope(badHashEnv);
    REQUIRE(parseSaveEnvelope(resealedBadHash).ok());
    auto loadedBadHash = loadCampaign(resealedBadHash, pack);
    CHECK_FALSE(loadedBadHash.ok());
    CHECK(loadedBadHash.err == WotrErrc::DamagedSaveChecksum);

    // Re-sealing with non-canonical stateBytes (extra unparsed field) fails even if
    // stateHash equals stateHash(parsedState).
    WotrSaveEnvelopeV1 nonCanonicalEnv = parsed.value;
    JsonValue extraInner = JsonValue::parse(nonCanonicalEnv.stateBytes);
    JsonValue extraVal;
    extraVal.type = JsonValue::Type::String;
    extraVal.stringValue = "injected";
    extraInner.objectValue.emplace("extraField", extraVal);
    nonCanonicalEnv.stateBytes = extraInner.canonical();
    nonCanonicalEnv.checksum = Sha256::hashHex(saveEnvelopePayload(nonCanonicalEnv));
    auto loadedNonCanonical = loadCampaign(serializeSaveEnvelope(nonCanonicalEnv), pack);
    CHECK_FALSE(loadedNonCanonical.ok());
    CHECK(loadedNonCanonical.err == WotrErrc::DamagedSaveChecksum);
}

TEST_CASE("save: stale envelope missing stateHash or unknown inner schemaVersion fails without fallback") {
    auto [pack, s] = resolvedState();
    const std::string envelope = saveCampaign(s, pack);

    // Stale envelope missing stateHash (even if outer checksum is recomputed over
    // the remaining fields) fails with InvalidSave — no silent migration/fallback.
    JsonValue stale = JsonValue::parse(envelope);
    stale.objectValue.erase("stateHash");
    JsonValue stalePayload = stale;
    stalePayload.objectValue.erase("checksum");
    stale.objectValue.at("checksum").stringValue = Sha256::hashHex(stalePayload.canonical());
    const std::string staleText = stale.canonical();
    auto staleParsed = parseSaveEnvelope(staleText);
    CHECK_FALSE(staleParsed.ok());
    CHECK(staleParsed.err == WotrErrc::InvalidSave);
    CHECK_FALSE(staleParsed.message.find("stateHash") == std::string::npos);
    auto staleLoaded = loadCampaign(staleText, pack);
    CHECK_FALSE(staleLoaded.ok());
    CHECK(staleLoaded.err == WotrErrc::InvalidSave);

    // Re-sealed envelope with unknown inner schemaVersion keeps failing with UnknownSaveVersion.
    auto parsed = parseSaveEnvelope(envelope);
    REQUIRE(parsed.ok());
    WotrSaveEnvelopeV1 badSchema = parsed.value;
    JsonValue inner = JsonValue::parse(badSchema.stateBytes);
    inner.objectValue.at("schemaVersion").uintValue = 2;
    badSchema.stateBytes = inner.canonical();
    badSchema.checksum = Sha256::hashHex(saveEnvelopePayload(badSchema));
    auto badSchemaLoaded = loadCampaign(serializeSaveEnvelope(badSchema), pack);
    CHECK_FALSE(badSchemaLoaded.ok());
    CHECK(badSchemaLoaded.err == WotrErrc::UnknownSaveVersion);
}

TEST_CASE("save: A1 PROBE 3 reproducer (forged acceptedCommands[*].checksum = ffff...f) is refused and re-save keeps clean log") {
    auto packRes = loadPack(readFile(fixtureDir() + "/pack.json"));
    INFO(packRes.message);
    REQUIRE(packRes.ok());
    const Pack pack = packRes.value;

    auto st = initialState(pack, 7ull);
    INFO(st.message);
    REQUIRE(st.ok());
    auto r = resolveCommands(
        st.value,
        {moveEnvelopeTest(0, "faction_gondor", 1, "c1", "army_gondor", "crossing"),
         moveEnvelopeTest(0, "faction_mordor", 1, "c2", "army_mordor", "crossing")});
    INFO(r.message);
    REQUIRE(r.ok());
    REQUIRE(r.value.state.acceptedCommands.size() == 2u);

    // Active session holding the valid campaign state before attempting transactional load.
    CampaignState session = r.value.state;
    const std::string beforeBytes = canonicalState(session);
    const std::string beforeHash = stateHash(session);
    const std::string legitEnvelope = saveCampaign(session, pack);
    const std::string forgedDigest(64, 'f');
    REQUIRE(beforeBytes.find(forgedDigest) == std::string::npos);

    auto parsed = parseSaveEnvelope(legitEnvelope);
    INFO(parsed.message);
    REQUIRE(parsed.ok());

    // 1) Exact A1 PROBE 3 reproducer: replace every acceptedCommands[*].checksum
    //    with 64 'f' chars and re-seal outer envelope checksum.
    WotrSaveEnvelopeV1 probe3 = parsed.value;
    JsonValue inner = JsonValue::parse(probe3.stateBytes);
    for (auto& c : inner.objectValue.at("acceptedCommands").arrayValue) {
        c.objectValue.at("checksum").stringValue = forgedDigest;
    }
    probe3.stateBytes = inner.canonical();
    probe3.checksum = Sha256::hashHex(saveEnvelopePayload(probe3));
    const std::string probe3Text = serializeSaveEnvelope(probe3);
    REQUIRE(parseSaveEnvelope(probe3Text).ok());

    auto res1 = loadCampaign(probe3Text, pack);
    CHECK_FALSE(res1.ok());
    CHECK(res1.err == WotrErrc::DamagedChecksum);
    CHECK(std::string(errcName(res1.err)) == "ERR_DAMAGED_CHECKSUM");
    CHECK_FALSE(res1.message.empty());
    CHECK(res1.value.acceptedCommands.empty());
    if (res1.ok()) {
        session = res1.value;
    }

    // 2) Stronger post-Def-A1-1 variant: also recompute stateHash over the forged
    //    canonical stateBytes AND re-seal the outer envelope checksum.
    WotrSaveEnvelopeV1 probe3Rehashed = probe3;
    probe3Rehashed.stateHash = Sha256::hashHex(probe3Rehashed.stateBytes);
    probe3Rehashed.checksum = Sha256::hashHex(saveEnvelopePayload(probe3Rehashed));
    const std::string probe3RehashedText = serializeSaveEnvelope(probe3Rehashed);
    REQUIRE(parseSaveEnvelope(probe3RehashedText).ok());

    auto res2 = loadCampaign(probe3RehashedText, pack);
    CHECK_FALSE(res2.ok());
    CHECK(res2.err == WotrErrc::DamagedChecksum);
    CHECK(std::string(errcName(res2.err)) == "ERR_DAMAGED_CHECKSUM");
    CHECK_FALSE(res2.message.empty());
    CHECK(res2.value.acceptedCommands.empty());
    if (res2.ok()) {
        session = res2.value;
    }

    // Transactional invariant: session was not mutated by either refused load, and a
    // subsequent re-save preserves the legitimate log byte-for-byte without ffff...f.
    CHECK(canonicalState(session) == beforeBytes);
    CHECK(stateHash(session) == beforeHash);
    const std::string resaved = saveCampaign(session, pack);
    CHECK(resaved == legitEnvelope);
    auto reparsed = parseSaveEnvelope(resaved);
    REQUIRE(reparsed.ok());
    CHECK(reparsed.value.stateBytes.find(forgedDigest) == std::string::npos);
    CHECK(reparsed.value.stateBytes == beforeBytes);
}

TEST_CASE("save: acceptedCommands payload tampering with stale command checksum is refused at load") {
    auto [pack, s] = resolvedState();
    REQUIRE(s.acceptedCommands.size() == 2u);
    const std::string envelope = saveCampaign(s, pack);
    auto parsed = parseSaveEnvelope(envelope);
    REQUIRE(parsed.ok());

    // Rewrite only the second command's targetRegionId while keeping its old checksum,
    // then recompute both stateHash and outer envelope checksum.
    WotrSaveEnvelopeV1 tampered = parsed.value;
    JsonValue inner = JsonValue::parse(tampered.stateBytes);
    inner.objectValue.at("acceptedCommands")
        .arrayValue.at(1)
        .objectValue.at("payload")
        .objectValue.at("move")
        .objectValue.at("targetRegionId")
        .stringValue = "west";
    tampered.stateBytes = inner.canonical();
    tampered.stateHash = Sha256::hashHex(tampered.stateBytes);
    tampered.checksum = Sha256::hashHex(saveEnvelopePayload(tampered));

    auto loaded = loadCampaign(serializeSaveEnvelope(tampered), pack);
    CHECK_FALSE(loaded.ok());
    CHECK(loaded.err == WotrErrc::DamagedChecksum);
    CHECK(std::string(errcName(loaded.err)) == "ERR_DAMAGED_CHECKSUM");
    CHECK(loaded.value.acceptedCommands.empty());
}
