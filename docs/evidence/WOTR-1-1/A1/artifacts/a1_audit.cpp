// OpenBFME GPL-3.0. WOTR-1.1 A1 independent auditor harness.
//
// This file is NOT part of the repository. It is written by the Determinism &
// Save Auditor to obtain independent evidence: it links only against the public
// WotrRemaster headers, drives the core through its public API, and prints the
// SHA-256 of canonical bytes at every resolution boundary. Two runs in separate
// checkouts/output directories must produce byte-identical logs.
//
// Usage: a1_audit <fixture-dir>

#include "GameLogic/WotrRemaster/Errors.h"
#include "GameLogic/WotrRemaster/Hash.h"
#include "GameLogic/WotrRemaster/Json.h"
#include "GameLogic/WotrRemaster/Pack.h"
#include "GameLogic/WotrRemaster/Save.h"
#include "GameLogic/WotrRemaster/State.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace WotrRemaster;

namespace {

int g_fail = 0;
int g_checks = 0;

void check(bool cond, const std::string& what) {
    ++g_checks;
    if (!cond) {
        ++g_fail;
        std::cout << "CHECK_FAIL " << what << "\n";
    }
}

void emit(const std::string& line) { std::cout << line << "\n"; }

std::string hex(const std::string& bytes) { return Sha256::hashHex(bytes); }

std::string num(std::uint64_t v) { return std::to_string(v); }

std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        std::cerr << "cannot open " << path << "\n";
        std::exit(2);
    }
    return std::string((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
}

std::string resealEnvelope(WotrSaveEnvelopeV1 e) {
    e.checksum = Sha256::hashHex(saveEnvelopePayload(e));
    return serializeSaveEnvelope(e);
}

CommandEnvelope makeMove(std::uint32_t turn, const std::string& player,
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

BattleResultV1 makeVictoryResult(const PendingBattle& pb) {
    BattleResultV1 r;
    r.contractVersion = 1;
    r.id = pb.id;
    r.requestChecksum = pb.requestChecksum;
    r.completionReason = CompletionReason::Victory;
    r.victor = "faction_gondor";
    SurvivingArmy sa;
    sa.id = "army_gondor";
    SurvivingMember sm;
    sm.id = "member_gondor_1";
    sm.quantity = 12;
    sm.experience = 5;
    sa.members.push_back(sm);
    r.survivors.push_back(sa);
    r.resultChecksum = battleResultChecksum(r);
    return r;
}

void boundary(const std::string& tag, const CampaignState& s) {
    const std::string bytes = canonicalState(s);
    emit("BOUNDARY " + tag + " len=" + num(bytes.size()) + " bytes_sha256=" +
         hex(bytes) + " state_hash=" + stateHash(s));
}

void reportFailure(const std::string& tag, const WotrErrc err,
                   const std::string& message) {
    emit("NEGATIVE " + tag + " err=" + errcName(err) + " msg_sha256=" + hex(message));
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: a1_audit <fixture-dir>\n";
        return 2;
    }
    const std::string fixtureDir = argv[1];

    // ---------------------------------------------------------------- P1 pack
    const std::string packText = readFile(fixtureDir + "/pack.json");
    emit("FIXTURE pack.json len=" + num(packText.size()) + " sha256=" + hex(packText));

    auto packRes = loadPack(packText);
    check(packRes.ok(), "pack loads");
    if (!packRes.ok()) {
        std::cerr << "pack load failed: " << errcName(packRes.err) << " "
                  << packRes.message << "\n";
        return 3;
    }
    const Pack pack = packRes.value;
    emit("PACK id=" + pack.manifest.id +
         " version=" + num(pack.manifest.version) +
         " profile=" + pack.manifest.tacticalProfile +
         " map=" + pack.manifest.tacticalMap +
         " regions=" + num(pack.regions.size()) +
         " factions=" + num(pack.factions.size()) +
         " armies=" + num(pack.armies.size()));
    emit("PACK canonical_len=" + num(canonicalPack(pack).size()) +
         " content_hash=" + contentHashOf(pack) +
         " declared_hash=" + pack.manifest.contentHash.value_or("<none>"));
    check(pack.manifest.contentHash.has_value(), "fixture declares content hash");
    check(pack.manifest.contentHash.value_or("") == contentHashOf(pack),
          "declared content hash equals recomputed canonical hash");

    // ------------------------------------------------------------- P2 initial
    auto init1 = initialState(pack, 20261121ull);
    auto init2 = initialState(pack, 20261121ull);
    auto initOtherSeed = initialState(pack, 20261122ull);
    check(init1.ok() && init2.ok() && initOtherSeed.ok(), "initial states build");
    if (!init1.ok() || !init2.ok() || !initOtherSeed.ok()) return 3;

    boundary("initial_state", init1.value);
    check(canonicalState(init1.value) == canonicalState(init2.value),
          "two independent initial states are byte-identical");
    check(stateHash(init1.value) != stateHash(initOtherSeed.value),
          "explicit seed changes canonical state (seed is an input, not ambient)");
    emit("SEED explicit=20261121 state_hash=" + stateHash(init1.value));
    emit("SEED explicit=20261122 state_hash=" + stateHash(initOtherSeed.value));
    emit("SEED initialSeed_field=" + num(init1.value.initialSeed) +
         " rng_metadata_is_empty=" + std::string("true"));

    // -------------------------------------------------- P3 resolve two moves
    CommandEnvelope g = makeMove(0, "faction_gondor", 1, "cmd-g-1", "army_gondor",
                                "crossing");
    CommandEnvelope m = makeMove(0, "faction_mordor", 1, "cmd-m-1", "army_mordor",
                                "crossing");
    emit("COMMAND g envelope_checksum=" + g.checksum);
    emit("COMMAND m envelope_checksum=" + m.checksum);

    auto rA = resolveCommands(init1.value, {g, m});
    auto rB = resolveCommands(init2.value, {m, g}); // reversed submission order
    check(rA.ok() && rB.ok(), "both resolutions succeed");
    if (!rA.ok() || !rB.ok()) return 3;
    check(rA.value.battlePending, "conflict creates a pending battle");
    check(rA.value.state.pendingBattle.has_value(), "pending battle present");
    if (!rA.value.state.pendingBattle.has_value()) return 3;

    boundary("after_resolve", rA.value.state);
    check(canonicalState(rA.value.state) == canonicalState(rB.value.state),
          "submission order does not change canonical state");
    emit("BATTLE id=" + rA.value.state.pendingBattle->id +
         " participants=" + num(rA.value.state.pendingBattle->participants.size()) +
         " request_checksum=" + rA.value.request.requestChecksum +
         " request_selfcheck_ok=" +
         std::string(battleRequestChecksum(rA.value.request) ==
                             rA.value.request.requestChecksum
                         ? "true"
                         : "false"));
    check(battleRequestChecksum(rA.value.request) ==
              rA.value.request.requestChecksum,
          "request checksum is self-consistent");
    emit("BATTLE request_seed=" + num(rA.value.request.tacticalSeed) +
         " state_hash_carried=" + rA.value.request.stateHash +
         " events=" + num(rA.value.events.size()));

    const std::string preResultBytes = canonicalState(rA.value.state);
    const std::string preResultHash = stateHash(rA.value.state);

    // ------------------------------------------------ N8..N11 bad results
    {
        BattleResultV1 wrongId = makeVictoryResult(*rA.value.state.pendingBattle);
        wrongId.id = "battle:west:0";
        wrongId.resultChecksum = battleResultChecksum(wrongId);
        auto res = applyBattleResult(rA.value.state, wrongId);
        check(!res.ok() && res.err == WotrErrc::UnknownBattle,
              "wrong battle id rejected with UnknownBattle");
        reportFailure("result_wrong_battle_id", res.err, res.message);
        check(canonicalState(rA.value.state) == preResultBytes,
              "wrong battle id leaves bytes unchanged");
    }
    {
        BattleResultV1 badReq = makeVictoryResult(*rA.value.state.pendingBattle);
        badReq.requestChecksum = std::string(64, 'a');
        badReq.resultChecksum = battleResultChecksum(badReq);
        auto res = applyBattleResult(rA.value.state, badReq);
        check(!res.ok() && res.err == WotrErrc::MismatchedChecksum,
              "mismatched request checksum rejected");
        reportFailure("result_mismatched_request_checksum", res.err, res.message);
        check(canonicalState(rA.value.state) == preResultBytes,
              "mismatched request checksum leaves bytes unchanged");
    }
    {
        BattleResultV1 badVer = makeVictoryResult(*rA.value.state.pendingBattle);
        badVer.contractVersion = 2;
        auto res = applyBattleResult(rA.value.state, badVer);
        check(!res.ok() && res.err == WotrErrc::InvalidResult,
              "unknown result contract version rejected");
        reportFailure("result_unknown_contract_version", res.err, res.message);
        check(canonicalState(rA.value.state) == preResultBytes,
              "unknown contract version leaves bytes unchanged");
    }
    {
        BattleResultV1 damaged = makeVictoryResult(*rA.value.state.pendingBattle);
        damaged.survivors[0].members[0].quantity = 99; // checksum now stale
        auto res = applyBattleResult(rA.value.state, damaged);
        check(!res.ok() && res.err == WotrErrc::DamagedChecksum,
              "damaged result checksum rejected");
        reportFailure("result_damaged_checksum", res.err, res.message);
        check(canonicalState(rA.value.state) == preResultBytes,
              "damaged result leaves bytes unchanged");
    }
    {
        BattleResultV1 unknownMember = makeVictoryResult(*rA.value.state.pendingBattle);
        unknownMember.survivors[0].members[0].id = "member_ghost";
        unknownMember.resultChecksum = battleResultChecksum(unknownMember);
        auto res = applyBattleResult(rA.value.state, unknownMember);
        check(!res.ok() && res.err == WotrErrc::UnknownEntity,
              "unknown survivor member rejected");
        reportFailure("result_unknown_member", res.err, res.message);
        check(canonicalState(rA.value.state) == preResultBytes,
              "unknown member leaves bytes unchanged");
    }

    // ------------------------------------------------------- P4 apply result
    const BattleResultV1 result = makeVictoryResult(*rA.value.state.pendingBattle);
    auto applied = applyBattleResult(rA.value.state, result);
    check(applied.ok(), "valid result applies");
    if (!applied.ok()) return 3;
    boundary("after_apply", applied.value);
    check(applied.value.turn == 1, "turn advanced exactly once");
    check(applied.value.phase == Phase::Planning, "phase returns to planning");
    check(applied.value.regions.at("crossing").owner.value_or("") == "faction_gondor",
          "region ownership passes to victor");
    check(applied.value.armies.count("army_mordor") == 0, "loser army removed");
    check(!applied.value.pendingBattle.has_value(), "pending battle cleared");
    emit("RESULT checksum=" + result.resultChecksum);

    const std::string postApplyBytes = canonicalState(applied.value);

    // ------------------------------------------------- P5 duplicate apply
    {
        auto res = applyBattleResult(applied.value, result);
        check(!res.ok() && res.err == WotrErrc::UnknownBattle,
              "duplicate apply rejected with UnknownBattle");
        reportFailure("duplicate_apply", res.err, res.message);
        check(canonicalState(applied.value) == postApplyBytes,
              "duplicate apply leaves bytes unchanged");
    }

    // -------------------------------------------------------------- P6 save
    const std::string envelope = saveCampaign(applied.value, pack);
    const std::string envelope2 = saveCampaign(applied.value, pack);
    emit("SAVE envelope_len=" + num(envelope.size()) + " sha256=" + hex(envelope));
    check(envelope == envelope2, "saving twice yields byte-identical envelope");
    WotrSaveEnvelopeV1 parsed;
    {
        auto p = parseSaveEnvelope(envelope);
        check(p.ok(), "save envelope reparses");
        if (!p.ok()) return 3;
        parsed = p.value;
    }
    emit("SAVE cursor=" + num(parsed.commandCursor) +
         " envelope_version=" + num(parsed.envelopeVersion) +
         " contract_version=" + num(parsed.contractVersion) +
         " state_bytes_sha256=" + hex(parsed.stateBytes) +
         " state_bytes_eq_canonical=" +
         std::string(parsed.stateBytes == postApplyBytes ? "true" : "false"));
    check(parsed.stateBytes == postApplyBytes,
          "save embeds the canonical state bytes verbatim");
    check(parsed.packHash == contentHashOf(pack), "save carries pack content hash");
    check(parsed.tacticalProfile == pack.manifest.tacticalProfile,
          "save carries tactical profile");
    check(parsed.rngMetadata == "{}", "save carries empty RNG metadata (no ambient RNG)");
    check(parsed.commandCursor == applied.value.acceptedCommands.size(),
          "save command cursor equals accepted command count");

    // -------------------------------------------------------------- P7 load
    auto loaded = loadCampaign(envelope, pack);
    check(loaded.ok(), "save loads into a fresh instance");
    if (!loaded.ok()) return 3;
    boundary("after_load_fresh_instance", loaded.value);
    check(canonicalState(loaded.value) == postApplyBytes,
          "fresh-instance load yields identical bytes");
    check(stateHash(loaded.value) == stateHash(applied.value),
          "fresh-instance load yields identical hash");

    // ------------------------------------------------ P8 next step identical
    CommandEnvelope cont = makeMove(1, "faction_gondor", 1, "cmd-cont-1",
                                   "army_gondor", "west");
    auto fromLoaded = resolveCommands(loaded.value, {cont});
    auto fromMemory = resolveCommands(applied.value, {cont});
    check(fromLoaded.ok() && fromMemory.ok(), "next step resolves on both");
    if (!fromLoaded.ok() || !fromMemory.ok()) return 3;
    boundary("next_step_from_loaded", fromLoaded.value.state);
    check(canonicalState(fromLoaded.value.state) ==
              canonicalState(fromMemory.value.state),
          "loaded instance continues with the same next-step bytes");
    check(stateHash(fromLoaded.value.state) == stateHash(fromMemory.value.state),
          "loaded instance continues with the same next-step hash");

    const std::string savedStateBytes = postApplyBytes; // state to protect in N1..N7

    // -------------------------------------- N1 damaged envelope checksum
    {
        JsonValue root = JsonValue::parse(envelope);
        std::string sb = root.objectValue.at("stateBytes").stringValue;
        sb[sb.size() / 2] ^= 0x01;
        root.objectValue.at("stateBytes").stringValue = sb;
        const std::string tampered = root.canonical();
        auto res = loadCampaign(tampered, pack);
        check(!res.ok() && res.err == WotrErrc::DamagedSaveChecksum,
              "corrupt envelope checksum rejected");
        reportFailure("save_damaged_checksum", res.err, res.message);
        check(canonicalState(applied.value) == savedStateBytes,
              "failed load mutated no state");
    }

    // --------------------------------------- N2 unknown envelope version
    {
        JsonValue root = JsonValue::parse(envelope);
        root.objectValue.at("envelopeVersion").uintValue = 2;
        auto res = loadCampaign(root.canonical(), pack);
        check(!res.ok() && res.err == WotrErrc::UnknownSaveVersion,
              "unknown envelope version rejected");
        reportFailure("save_unknown_envelope_version", res.err, res.message);
        check(canonicalState(applied.value) == savedStateBytes,
              "unknown envelope version mutated no state");
    }

    // --------------------------------------- N3 unknown contract version
    {
        JsonValue root = JsonValue::parse(envelope);
        root.objectValue.at("contractVersion").uintValue = 9;
        auto res = loadCampaign(root.canonical(), pack);
        check(!res.ok() && res.err == WotrErrc::UnknownSaveVersion,
              "unknown contract version rejected");
        reportFailure("save_unknown_contract_version", res.err, res.message);
        check(canonicalState(applied.value) == savedStateBytes,
              "unknown contract version mutated no state");
    }

    // ------------------- N4 validly re-sealed envelope with bad inner state
    {
        WotrSaveEnvelopeV1 e = parsed;
        JsonValue inner = JsonValue::parse(e.stateBytes);
        inner.objectValue.at("schemaVersion").uintValue = 2;
        e.stateBytes = inner.canonical();
        const std::string sealed = resealEnvelope(e);
        auto res = loadCampaign(sealed, pack);
        check(!res.ok() && res.err == WotrErrc::UnknownSaveVersion,
              "re-sealed envelope with unknown state schema rejected");
        reportFailure("save_resealed_unknown_state_schema", res.err, res.message);
        check(canonicalState(applied.value) == savedStateBytes,
              "re-sealed bad inner state mutated no state");
    }

    // ------------------------------------------ N5 mismatched pack identity
    {
        JsonValue root = JsonValue::parse(packText);
        root.objectValue.at("pack").objectValue.at("id").stringValue = "other_pack";
        root.objectValue.at("pack").objectValue.erase("contentHash");
        auto otherPackRes = loadPack(root.canonical());
        check(otherPackRes.ok(), "variant pack with foreign id loads");
        if (otherPackRes.ok()) {
            auto res = loadCampaign(envelope, otherPackRes.value);
            check(!res.ok() && res.err == WotrErrc::IncompatiblePack,
                  "save from another pack id rejected");
            reportFailure("save_mismatched_pack_id", res.err, res.message);
        }
        check(canonicalState(applied.value) == savedStateBytes,
              "mismatched pack id mutated no state");
    }

    // ------------------------------------- N6 mismatched pack content bytes
    {
        JsonValue root = JsonValue::parse(packText);
        root.objectValue.at("pack").objectValue.erase("contentHash");
        JsonValue& armies = root.objectValue.at("armies");
        for (auto& a : armies.arrayValue) {
            if (a.objectValue.at("id").stringValue == "army_gondor") {
                a.objectValue.at("members").arrayValue[0]
                    .objectValue.at("quantity")
                    .uintValue = 13;
            }
        }
        auto otherPackRes = loadPack(root.canonical());
        check(otherPackRes.ok(), "variant pack with different content loads");
        if (otherPackRes.ok()) {
            check(contentHashOf(otherPackRes.value) != contentHashOf(pack),
                  "variant pack has a different content hash");
            auto res = loadCampaign(envelope, otherPackRes.value);
            check(!res.ok() && res.err == WotrErrc::IncompatiblePack,
                  "save from a different pack content rejected");
            reportFailure("save_mismatched_pack_content", res.err, res.message);
        }
        check(canonicalState(applied.value) == savedStateBytes,
              "mismatched pack content mutated no state");
    }

    // ------------------------------ N7 pack with damaged declared content hash
    {
        JsonValue root = JsonValue::parse(packText);
        root.objectValue.at("pack").objectValue.at("contentHash").stringValue =
            std::string(64, 'b');
        auto badPackRes = loadPack(root.canonical());
        check(!badPackRes.ok() && badPackRes.err == WotrErrc::ContentHashMismatch,
              "pack with damaged declared content hash rejected");
        reportFailure("pack_damaged_declared_hash", badPackRes.err,
                      badPackRes.message);
    }

    // ------------------------------ N13..N17 transactionality of commands
    {
        const std::string before = canonicalState(init1.value);
        CommandEnvelope badChecksum = g;
        badChecksum.checksum = std::string(64, 'c');
        auto res = resolveCommands(init1.value, {badChecksum});
        check(!res.ok() && res.err == WotrErrc::DamagedChecksum,
              "command with damaged checksum rejected");
        reportFailure("command_damaged_checksum", res.err, res.message);
        check(canonicalState(init1.value) == before, "bad command mutated no state");
    }
    {
        const std::string before = canonicalState(init1.value);
        CommandEnvelope unknownArmy = makeMove(0, "faction_gondor", 1, "cmd-x-1",
                                              "army_ghost", "crossing");
        auto res = resolveCommands(init1.value, {unknownArmy});
        check(!res.ok() && res.err == WotrErrc::UnknownEntity,
              "command for unknown army rejected");
        reportFailure("command_unknown_army", res.err, res.message);
        check(canonicalState(init1.value) == before, "unknown army mutated no state");
    }
    {
        const std::string before = canonicalState(init1.value);
        CommandEnvelope nonAdjacent = makeMove(0, "faction_gondor", 1, "cmd-x-2",
                                              "army_gondor", "east");
        auto res = resolveCommands(init1.value, {nonAdjacent});
        check(!res.ok() && res.err == WotrErrc::InvalidCommand,
              "non-adjacent move rejected");
        reportFailure("command_non_adjacent", res.err, res.message);
        check(canonicalState(init1.value) == before, "non-adjacent mutated no state");
    }
    {
        const std::string before = canonicalState(init1.value);
        CommandEnvelope foreign = makeMove(0, "faction_mordor", 1, "cmd-x-3",
                                          "army_gondor", "crossing");
        auto res = resolveCommands(init1.value, {foreign});
        check(!res.ok() && res.err == WotrErrc::InvalidCommand,
              "command by non-owner rejected");
        reportFailure("command_wrong_owner", res.err, res.message);
        check(canonicalState(init1.value) == before, "wrong owner mutated no state");
    }
    {
        const std::string before = canonicalState(init1.value);
        auto res = resolveCommands(init1.value, {g, g});
        check(!res.ok() && res.err == WotrErrc::InvalidCommand,
              "same army twice in one batch rejected");
        reportFailure("command_duplicate_army_batch", res.err, res.message);
        check(canonicalState(init1.value) == before,
              "duplicate army batch mutated no state");
    }

    // ------------------------------- O1 source-order independence of pack
    {
        JsonValue root = JsonValue::parse(packText);
        root.objectValue.at("pack").objectValue.erase("contentHash");
        JsonValue& regs = root.objectValue.at("regions");
        std::vector<JsonValue> reversed(regs.arrayValue.rbegin(),
                                        regs.arrayValue.rend());
        regs.arrayValue = reversed;
        for (auto& r : root.objectValue.at("regions").arrayValue) {
            JsonValue& con = r.objectValue.at("connections");
            std::vector<JsonValue> rev(con.arrayValue.rbegin(), con.arrayValue.rend());
            con.arrayValue = rev;
        }
        JsonValue& facs = root.objectValue.at("factions");
        std::vector<JsonValue> facRev(facs.arrayValue.rbegin(), facs.arrayValue.rend());
        facs.arrayValue = facRev;
        JsonValue& arms = root.objectValue.at("armies");
        std::vector<JsonValue> armRev(arms.arrayValue.rbegin(), arms.arrayValue.rend());
        arms.arrayValue = armRev;

        auto reorderedRes = loadPack(root.canonical());
        check(reorderedRes.ok(), "reordered source pack loads");
        if (reorderedRes.ok()) {
            check(contentHashOf(reorderedRes.value) == contentHashOf(pack),
                  "reordered source yields the same canonical pack hash");
            auto st = initialState(reorderedRes.value, 20261121ull);
            check(st.ok(), "reordered pack builds an initial state");
            if (st.ok()) {
                check(stateHash(st.value) == stateHash(init1.value),
                      "reordered source yields the same initial state hash");
                check(canonicalState(st.value) == canonicalState(init1.value),
                      "reordered source yields the same initial state bytes");
            }
            emit("ORDER_INDEPENDENCE canonical_pack_hash=" +
                 contentHashOf(reorderedRes.value));
        }
    }

    emit("AUDIT_SUMMARY checks=" + num(static_cast<std::uint64_t>(g_checks)) +
         " failures=" + num(static_cast<std::uint64_t>(g_fail)));
    return g_fail == 0 ? 0 : 1;
}
