// OpenBFME. GPL-3.0.
// WotrRemaster deterministic strategic core: state, commands and resolution tests.

#include <doctest.h>

#include "test_common.h"

#include "GameLogic/WotrRemaster/Pack.h"
#include "GameLogic/WotrRemaster/State.h"

using namespace WotrRemaster;
using namespace WotrRemasterTest;

namespace {

// A fully validated pack and a fresh initial state (seed is explicit).
std::pair<Pack, CampaignState> freshState(std::uint64_t seed = 42u) {
    auto packRes = loadPack(readFile(fixtureDir() + "/pack.json"));
    INFO(packRes.message);
    REQUIRE(packRes.ok());
    Pack pack = packRes.value;
    auto st = initialState(pack, seed);
    INFO(st.message);
    REQUIRE(st.ok());
    return {pack, st.value};
}

// Build a MoveArmy envelope with a valid checksum.
CommandEnvelope moveEnvelope(std::uint32_t turn, const std::string& player,
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

TEST_CASE("state: initial state from the tiny pack") {
    auto [pack, s] = freshState();
    CHECK(s.turn == 0);
    CHECK(s.phase == Phase::Planning);
    CHECK(s.packId == "wotr_1_1_tiny");
    CHECK(s.packHash == contentHashOf(pack));
    CHECK(s.tacticalProfile == "vanilla_rotwk_201");
    CHECK(s.tacticalMap == "helms_deep");
    CHECK(s.factions.size() == 2);
    CHECK(s.regions.size() == 3);
    CHECK(s.armies.size() == 2);
    CHECK(s.armies.at("army_gondor").regionId == "west");
    CHECK(s.armies.at("army_mordor").regionId == "east");
    CHECK_FALSE(s.pendingBattle.has_value());
}

TEST_CASE("state: initial state hash is deterministic") {
    auto s1 = initialState(loadPack(readFile(fixtureDir() + "/pack.json")).value, 7u);
    auto s2 = initialState(loadPack(readFile(fixtureDir() + "/pack.json")).value, 7u);
    REQUIRE(s1.ok()); REQUIRE(s2.ok());
    CHECK(canonicalState(s1.value) == canonicalState(s2.value));
    CHECK(stateHash(s1.value) == stateHash(s2.value));
}

TEST_CASE("state: two fresh instances processing the same commands stay equal") {
    auto [pack, sA] = freshState();
    auto stB = initialState(pack, 42u);
    REQUIRE(stB.ok());
    CampaignState& sB = stB.value;

    CommandEnvelope g = moveEnvelope(0, "faction_gondor", 1, "cmd-g-1", "army_gondor",
                                     "crossing");
    CommandEnvelope m = moveEnvelope(0, "faction_mordor", 1, "cmd-m-1", "army_mordor",
                                     "crossing");

    auto ra = resolveCommands(sA, {g, m});
    auto rb = resolveCommands(sB, {m, g}); // different submission order
    REQUIRE(ra.ok()); REQUIRE(rb.ok());
    CHECK(ra.value.battlePending);
    CHECK(rb.value.battlePending);
    // Canonical order makes resolution order-independent.
    CHECK(canonicalState(ra.value.state) == canonicalState(rb.value.state));
    CHECK(stateHash(ra.value.state) == stateHash(rb.value.state));
    CHECK(ra.value.state.pendingBattle->id == rb.value.state.pendingBattle->id);
    CHECK(ra.value.request.id == rb.value.request.id);
    CHECK(battleRequestChecksum(ra.value.request) == ra.value.request.requestChecksum);
    CHECK(battleRequestChecksum(rb.value.request) == rb.value.request.requestChecksum);

    // One pending battle, both armies in crossing.
    CHECK(ra.value.state.pendingBattle->participants.size() == 2);
    CHECK(ra.value.state.armies.at("army_gondor").regionId == "crossing");
    CHECK(ra.value.state.armies.at("army_mordor").regionId == "crossing");
}

TEST_CASE("commands: invalid command leaves state bytes equal") {
    auto [pack, s] = freshState();
    std::string before = canonicalState(s);

    // Unknown army.
    auto bad = moveEnvelope(0, "faction_gondor", 1, "cmd-bad", "army_nope", "crossing");
    auto res = resolveCommands(s, {bad});
    CHECK_FALSE(res.ok());
    CHECK(res.err == WotrErrc::UnknownEntity);
    CHECK(canonicalState(s) == before);
    CHECK(stateHash(s) == stateHash(s)); // hash stable

    // Non-adjacent target: west is not connected to east.
    auto far = moveEnvelope(0, "faction_gondor", 1, "cmd-far", "army_gondor", "east");
    res = resolveCommands(s, {far});
    CHECK_FALSE(res.ok());
    CHECK(res.err == WotrErrc::InvalidCommand);
    CHECK(canonicalState(s) == before);

    // Damaged envelope checksum.
    auto broken = moveEnvelope(0, "faction_gondor", 1, "cmd-broken", "army_gondor",
                               "crossing");
    broken.checksum = std::string(64, 'f');
    res = resolveCommands(s, {broken});
    CHECK_FALSE(res.ok());
    CHECK(res.err == WotrErrc::DamagedChecksum);
    CHECK(canonicalState(s) == before);

    // Wrong turn.
    auto late = moveEnvelope(1, "faction_gondor", 1, "cmd-late", "army_gondor", "crossing");
    res = resolveCommands(s, {late});
    CHECK_FALSE(res.ok());
    CHECK(res.err == WotrErrc::InvalidCommand);
    CHECK(canonicalState(s) == before);

    // Army already moved this turn.
    auto first = moveEnvelope(0, "faction_gondor", 1, "cmd-first", "army_gondor", "crossing");
    res = resolveCommands(s, {first});
    REQUIRE(res.ok());
    auto second = moveEnvelope(0, "faction_gondor", 2, "cmd-second", "army_gondor", "west");
    res = resolveCommands(res.value.state, {second});
    CHECK_FALSE(res.ok());
    CHECK(res.err == WotrErrc::InvalidCommand);
}

TEST_CASE("result: valid apply updates state once") {
    auto [pack, s] = freshState();
    CommandEnvelope g = moveEnvelope(0, "faction_gondor", 1, "cmd-g-1", "army_gondor",
                                     "crossing");
    CommandEnvelope m = moveEnvelope(0, "faction_mordor", 1, "cmd-m-1", "army_mordor",
                                     "crossing");
    auto res = resolveCommands(s, {g, m});
    REQUIRE(res.ok()); REQUIRE(res.value.battlePending);

    BattleResultV1 result;
    result.contractVersion = 1;
    result.id = res.value.state.pendingBattle->id;
    result.requestChecksum = res.value.state.pendingBattle->requestChecksum;
    result.completionReason = CompletionReason::Victory;
    result.victor = "faction_gondor";
    // Gondor survives intact; Mordor is destroyed (no survivor entry).
    SurvivingArmy sa;
    sa.id = "army_gondor";
    SurvivingMember sm;
    sm.id = "member_gondor_1";
    sm.quantity = 12;
    sm.experience = 1;
    sa.members.push_back(sm);
    result.survivors.push_back(sa);
    result.resultChecksum = battleResultChecksum(result);

    auto applied = applyBattleResult(res.value.state, result);
    REQUIRE(applied.ok());
    CHECK_FALSE(applied.value.pendingBattle.has_value());
    CHECK(applied.value.phase == Phase::Planning);
    CHECK(applied.value.turn == 1);
    CHECK(applied.value.regions.at("crossing").owner == "faction_gondor");
    REQUIRE(applied.value.armies.size() == 1);
    CHECK(applied.value.armies.count("army_mordor") == 0);
    CHECK(applied.value.armies.at("army_gondor").members.size() == 1);
    CHECK(applied.value.armies.at("army_gondor").members.at(0).experience == 1);
}

TEST_CASE("result: duplicate apply is rejected, state unchanged") {
    auto [pack, s] = freshState();
    CommandEnvelope g = moveEnvelope(0, "faction_gondor", 1, "cmd-g-1", "army_gondor",
                                     "crossing");
    CommandEnvelope m = moveEnvelope(0, "faction_mordor", 1, "cmd-m-1", "army_mordor",
                                     "crossing");
    auto res = resolveCommands(s, {g, m});
    REQUIRE(res.ok());

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
    sa.members.push_back(sm);
    result.survivors.push_back(sa);
    result.resultChecksum = battleResultChecksum(result);

    auto applied = applyBattleResult(res.value.state, result);
    REQUIRE(applied.ok());
    // State after the successful apply is the reference for "unchanged".
    std::string before = canonicalState(applied.value);
    // Second apply of the same battle: no pending battle left.
    auto again = applyBattleResult(applied.value, result);
    CHECK_FALSE(again.ok());
    CHECK(again.err == WotrErrc::UnknownBattle);
    // A failed apply is transactional: applied.value is unchanged.
    CHECK(canonicalState(applied.value) == before);
    CHECK(stateHash(applied.value) == stateHash(applied.value));
}

TEST_CASE("result: mismatched request checksum is rejected, state unchanged") {
    auto [pack, s] = freshState();
    CommandEnvelope g = moveEnvelope(0, "faction_gondor", 1, "cmd-g-1", "army_gondor",
                                     "crossing");
    CommandEnvelope m = moveEnvelope(0, "faction_mordor", 1, "cmd-m-1", "army_mordor",
                                     "crossing");
    auto res = resolveCommands(s, {g, m});
    REQUIRE(res.ok());

    std::string before = canonicalState(res.value.state);
    BattleResultV1 result;
    result.id = res.value.state.pendingBattle->id;
    result.requestChecksum = std::string(64, 'f'); // wrong
    result.resultChecksum = battleResultChecksum(result);
    auto applied = applyBattleResult(res.value.state, result);
    CHECK_FALSE(applied.ok());
    CHECK(applied.err == WotrErrc::MismatchedChecksum);
    CHECK(canonicalState(res.value.state) == before);
}

TEST_CASE("result: unknown battle id is rejected") {
    auto [pack, s] = freshState();
    BattleResultV1 result;
    result.id = "battle:nope:0";
    result.requestChecksum = std::string(64, '0');
    result.resultChecksum = battleResultChecksum(result);
    auto applied = applyBattleResult(s, result);
    CHECK_FALSE(applied.ok());
    CHECK(applied.err == WotrErrc::UnknownBattle);
}

TEST_CASE("result: damaged result checksum is rejected") {
    auto [pack, s] = freshState();
    CommandEnvelope g = moveEnvelope(0, "faction_gondor", 1, "cmd-g-1", "army_gondor",
                                     "crossing");
    CommandEnvelope m = moveEnvelope(0, "faction_mordor", 1, "cmd-m-1", "army_mordor",
                                     "crossing");
    auto res = resolveCommands(s, {g, m});
    REQUIRE(res.ok());

    BattleResultV1 result;
    result.id = res.value.state.pendingBattle->id;
    result.requestChecksum = res.value.state.pendingBattle->requestChecksum;
    result.resultChecksum = std::string(64, 'f'); // damaged
    auto applied = applyBattleResult(res.value.state, result);
    CHECK_FALSE(applied.ok());
    CHECK(applied.err == WotrErrc::DamagedChecksum);
}
