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
