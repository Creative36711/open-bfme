// WOTR-1.1 A1 auditor: targeted probe of save-envelope integrity boundaries.
// Not part of the repository. Read-only w.r.t. the tree.

#include "GameLogic/WotrRemaster/Errors.h"
#include "GameLogic/WotrRemaster/Hash.h"
#include "GameLogic/WotrRemaster/Json.h"
#include "GameLogic/WotrRemaster/Pack.h"
#include "GameLogic/WotrRemaster/Save.h"
#include "GameLogic/WotrRemaster/State.h"

#include <fstream>
#include <iostream>
#include <string>

using namespace WotrRemaster;

static std::string readFile(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
}

static std::string reseal(WotrSaveEnvelopeV1 e) {
    e.checksum = Sha256::hashHex(saveEnvelopePayload(e));
    return serializeSaveEnvelope(e);
}

static CommandEnvelope move(std::uint32_t turn, const std::string& player,
                            const std::string& cid, const std::string& army,
                            const std::string& target) {
    CommandEnvelope e;
    e.schemaVersion = 1;
    e.campaignId = "wotr_1_1_tiny";
    e.turn = turn;
    e.playerId = player;
    e.clientSequence = 1;
    e.commandId = cid;
    e.payload.type = CommandType::MoveArmy;
    e.payload.move.armyId = army;
    e.payload.move.targetRegionId = target;
    e.checksum = envelopeChecksum(e);
    return e;
}

int main(int argc, char** argv) {
    const std::string dir = argv[1];
    auto packRes = loadPack(readFile(dir + "/pack.json"));
    if (!packRes.ok()) { std::cerr << "pack\n"; return 2; }
    const Pack pack = packRes.value;

    auto st = initialState(pack, 7ull);
    auto r = resolveCommands(st.value,
                             {move(0, "faction_gondor", "c1", "army_gondor", "crossing"),
                              move(0, "faction_mordor", "c2", "army_mordor", "crossing")});
    if (!r.ok()) { std::cerr << "resolve\n"; return 2; }

    const std::string envelope = saveCampaign(r.value.state, pack);
    auto parsed = parseSaveEnvelope(envelope);
    if (!parsed.ok()) { std::cerr << "parse\n"; return 2; }
    const std::string goodState = parsed.value.stateBytes;
    const std::string goodHash = Sha256::hashHex(goodState);
    std::cout << "BASELINE state_bytes_len=" << goodState.size()
              << " state_hash=" << goodHash << "\n";

    // ---- PROBE 1: validly re-sealed envelope whose embedded state dropped
    // the combatant armies. Every envelope-level check still passes.
    {
        WotrSaveEnvelopeV1 e = parsed.value;
        JsonValue inner = JsonValue::parse(goodState);
        inner.objectValue.at("armies").arrayValue.clear();
        inner.objectValue.at("regions").arrayValue.clear();
        e.stateBytes = inner.canonical();
        const std::string forged = reseal(e);
        auto res = loadCampaign(forged, pack);
        const std::string resState =
            res.ok() ? canonicalState(res.value) : std::string("<none>");
        std::cout << "PROBE1 dropped_armies_and_regions accepted="
                  << (res.ok() ? "true" : "false")
                  << " err=" << (res.ok() ? std::string("Ok") : errcName(res.err))
                  << " loaded_state_len=" << (res.ok() ? std::to_string(resState.size()) : "-")
                  << " loaded_state_hash=" << (res.ok() ? Sha256::hashHex(resState) : "-")
                  << " equals_original="
                  << (res.ok() ? (resState == goodState ? "true" : "false") : "-")
                  << "\n";
    }

    // ---- PROBE 2: validly re-sealed envelope whose embedded state moves an
    // army into a non-adjacent region (rules violation) and rewrites a label.
    {
        WotrSaveEnvelopeV1 e = parsed.value;
        JsonValue inner = JsonValue::parse(goodState);
        for (auto& a : inner.objectValue.at("armies").arrayValue) {
            if (a.objectValue.at("id").stringValue == "army_gondor") {
                a.objectValue.at("regionId").stringValue = "east";
            }
        }
        e.stateBytes = inner.canonical();
        auto res = loadCampaign(reseal(e), pack);
        std::cout << "PROBE2 teleported_army accepted="
                  << (res.ok() ? "true" : "false")
                  << " err=" << (res.ok() ? std::string("Ok") : errcName(res.err))
                  << " loaded_army_region="
                  << (res.ok() ? res.value.armies.at("army_gondor").regionId : "-")
                  << "\n";
    }

    // ---- PROBE 3: validly re-sealed envelope with a forged command log
    // (envelope commandCursor still says 2, embedded commands claim a move
    // that never happened, with a checksum that does not verify).
    {
        WotrSaveEnvelopeV1 e = parsed.value;
        JsonValue inner = JsonValue::parse(goodState);
        for (auto& c : inner.objectValue.at("acceptedCommands").arrayValue) {
            c.objectValue.at("checksum").stringValue = std::string(64, 'f');
        }
        e.stateBytes = inner.canonical();
        auto res = loadCampaign(reseal(e), pack);
        std::cout << "PROBE3 forged_command_checksums accepted="
                  << (res.ok() ? "true" : "false")
                  << " err=" << (res.ok() ? std::string("Ok") : errcName(res.err))
                  << "\n";
        if (res.ok()) {
            // Does the forged log survive a re-save (i.e. become the new truth)?
            const std::string resaved = saveCampaign(res.value, pack);
            auto reparsed = parseSaveEnvelope(resaved);
            std::cout << "PROBE3 forged_log_resaved_checksum="
                      << (reparsed.ok()
                              ? reparsed.value.stateBytes.find(std::string(64, 'f')) !=
                                    std::string::npos
                                ? std::string("still_forged")
                                : std::string("rewritten")
                              : std::string("-"))
                      << "\n";
        }
    }

    // ---- PROBE 4: validly re-sealed envelope with a pending battle whose
    // requestChecksum was replaced. Load must not accept a battle whose
    // request cannot be reproduced.
    {
        WotrSaveEnvelopeV1 e = parsed.value;
        JsonValue inner = JsonValue::parse(goodState);
        inner.objectValue.at("pendingBattle")
            .objectValue.at("requestChecksum")
            .stringValue = std::string(64, '0');
        e.stateBytes = inner.canonical();
        auto res = loadCampaign(reseal(e), pack);
        std::cout << "PROBE4 forged_pending_request_checksum accepted="
                  << (res.ok() ? "true" : "false")
                  << " err=" << (res.ok() ? std::string("Ok") : errcName(res.err))
                  << "\n";
        if (res.ok()) {
            // A result sealed against the real request is now rejected?
            BattleResultV1 real;
            real.contractVersion = 1;
            real.id = r.value.state.pendingBattle->id;
            real.requestChecksum = r.value.state.pendingBattle->requestChecksum;
            real.completionReason = CompletionReason::Victory;
            real.victor = "faction_gondor";
            SurvivingArmy sa;
            sa.id = "army_gondor";
            SurvivingMember sm;
            sm.id = "member_gondor_1";
            sm.quantity = 12;
            sa.members.push_back(sm);
            real.survivors.push_back(sa);
            real.resultChecksum = battleResultChecksum(real);
            auto applied = applyBattleResult(res.value, real);
            std::cout << "PROBE4 legit_result_on_forged_state ok="
                      << (applied.ok() ? "true" : "false")
                      << " err="
                      << (applied.ok() ? std::string("Ok") : errcName(applied.err))
                      << "\n";
        }
    }

    // ---- PROBE 5: does a legitimate envelope load give the same bytes?
    {
        auto res = loadCampaign(envelope, pack);
        std::cout << "PROBE5 legit_load ok=" << (res.ok() ? "true" : "false")
                  << " bytes_equal="
                  << (res.ok() ? (canonicalState(res.value) == goodState ? "true" : "false")
                               : "-")
                  << "\n";
    }
    return 0;
}
