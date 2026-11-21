// OpenBFME. GPL-3.0.
// WotrRemaster deterministic strategic core: cross-process determinism tests.
// Simulates two fresh processes running the same pack, seed and commands and
// comparing canonical bytes and hash at every resolution boundary.

#include <doctest.h>

#include "test_common.h"

#include "GameLogic/WotrRemaster/Pack.h"
#include "GameLogic/WotrRemaster/Save.h"
#include "GameLogic/WotrRemaster/State.h"

using namespace WotrRemaster;
using namespace WotrRemasterTest;

namespace {

CommandEnvelope moveEnv(std::uint32_t turn, const std::string& player,
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

void assertBoundariesEqual(const CampaignState& a, const CampaignState& b) {
    CHECK(canonicalState(a) == canonicalState(b));
    CHECK(stateHash(a) == stateHash(b));
}

} // namespace

TEST_CASE("determinism: two fresh processes, equal bytes/hash at every boundary") {
    auto packRes = loadPack(readFile(fixtureDir() + "/pack.json"));
    INFO(packRes.message);
    REQUIRE(packRes.ok());
    Pack pack = packRes.value;

    // Process A.
    auto sa = initialState(pack, 20261121u);
    REQUIRE(sa.ok());
    // Process B: independently constructed, same inputs.
    auto sb = initialState(pack, 20261121u);
    REQUIRE(sb.ok());
    assertBoundariesEqual(sa.value, sb.value);

    CommandEnvelope g = moveEnv(0, "faction_gondor", 1, "cmd-g-1", "army_gondor",
                                "crossing");
    CommandEnvelope m = moveEnv(0, "faction_mordor", 1, "cmd-m-1", "army_mordor",
                                "crossing");

    auto ra = resolveCommands(sa.value, {g, m});
    auto rb = resolveCommands(sb.value, {m, g}); // reversed submission order
    REQUIRE(ra.ok()); REQUIRE(rb.ok());
    assertBoundariesEqual(ra.value.state, rb.value.state);
    CHECK(ra.value.battlePending);
    CHECK(rb.value.battlePending);
    CHECK(ra.value.state.pendingBattle->id == rb.value.state.pendingBattle->id);
    CHECK(ra.value.request.requestChecksum == battleRequestChecksum(ra.value.request));
    CHECK(rb.value.request.requestChecksum == battleRequestChecksum(rb.value.request));

    // Apply the same native result on both sides.
    BattleResultV1 result;
    result.contractVersion = 1;
    result.id = ra.value.state.pendingBattle->id;
    result.requestChecksum = ra.value.state.pendingBattle->requestChecksum;
    result.completionReason = CompletionReason::Victory;
    result.victor = "faction_gondor";
    SurvivingArmy saSurv;
    saSurv.id = "army_gondor";
    SurvivingMember sm;
    sm.id = "member_gondor_1";
    sm.quantity = 12;
    sm.experience = 5;
    saSurv.members.push_back(sm);
    result.survivors.push_back(saSurv);
    result.resultChecksum = battleResultChecksum(result);

    auto pa = applyBattleResult(ra.value.state, result);
    auto pb = applyBattleResult(rb.value.state, result);
    REQUIRE(pa.ok()); REQUIRE(pb.ok());
    assertBoundariesEqual(pa.value, pb.value);
    CHECK(pa.value.turn == 1);
    CHECK(pa.value.regions.at("crossing").owner == "faction_gondor");
    CHECK(pa.value.armies.count("army_mordor") == 0);

    // Second apply on both: rejected identically.
    auto pa2 = applyBattleResult(pa.value, result);
    auto pb2 = applyBattleResult(pb.value, result);
    CHECK_FALSE(pa2.ok());
    CHECK_FALSE(pb2.ok());
    CHECK(pa2.err == WotrErrc::UnknownBattle);
    CHECK(pb2.err == WotrErrc::UnknownBattle);
    assertBoundariesEqual(pa.value, pb.value);

    // Save/load in yet another fresh instance continues identically.
    std::string envelope = saveCampaign(pa.value, pack);
    auto loaded = loadCampaign(envelope, pack);
    REQUIRE(loaded.ok());
    assertBoundariesEqual(loaded.value, pa.value);

    // Next step on both.
    CommandEnvelope cont = moveEnv(1, "faction_gondor", 1, "cmd-cont-1", "army_gondor",
                                   "west");
    auto la = resolveCommands(loaded.value, {cont});
    auto lb = resolveCommands(pa.value, {cont});
    REQUIRE(la.ok()); REQUIRE(lb.ok());
    assertBoundariesEqual(la.value.state, lb.value.state);
}
