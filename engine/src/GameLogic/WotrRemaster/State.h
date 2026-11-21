// OpenBFME. GPL-3.0.
// WotrRemaster deterministic strategic core: campaign state, WEGO commands,
// deterministic conflict resolution and the BattleRequestV1 / BattleResultV1
// contracts (§5-6 of the WOTR-1.1 mission spec).
//
// Everything here is pure deterministic logic: integers only, no wall clock,
// no locale, no Godot, no unordered iteration (std::map/std::set give sorted
// iteration, and canonical serialization sorts collections explicitly).

#pragma once

#include "Errors.h"
#include "Hash.h"
#include "Pack.h"

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace WotrRemaster {

enum class Phase : std::uint8_t { Planning = 0, Resolving = 1, BattlePending = 2 };

struct StateMember {
    MemberId id;
    CatalogId kind;              // unit template catalog id
    std::uint32_t quantity = 0;
    std::uint32_t experience = 0;
    std::vector<UpgradeId> persistentUpgrades;
};

struct StateArmy {
    ArmyId id;
    FactionId factionId;
    RegionId regionId;
    std::vector<StateMember> members;
};

struct StateRegion {
    RegionId id;
    CatalogId name;
    std::optional<FactionId> owner;
    std::vector<RegionId> connections;
};

struct StateFaction {
    FactionId id;
    CatalogId name;
};

struct PendingBattle {
    BattleId id;
    RegionId regionId;
    std::vector<ArmyId> participants;   // sorted
    std::string requestChecksum;        // sha-256 hex of the emitted BattleRequestV1
};

struct CommandPayloadMove {
    ArmyId armyId;
    RegionId targetRegionId;
};

enum class CommandType : std::uint8_t { MoveArmy = 1 };

struct CommandPayload {
    CommandType type = CommandType::MoveArmy;
    CommandPayloadMove move;
};

struct CommandEnvelope {
    std::uint32_t schemaVersion = 1;
    PackId campaignId;
    std::uint32_t turn = 0;
    PlayerId playerId;
    std::uint64_t clientSequence = 0;
    CommandId commandId;
    CommandPayload payload;
    std::string checksum;  // sha-256 hex of canonical envelope fields excluding this one
};

// Canonical checksum of an envelope (without its checksum field).
std::string envelopeChecksum(const CommandEnvelope& e);

// BattleRequestV1: immutable once launched. Every §6 field is present;
// fields the current tactical runtime cannot produce are explicitly empty
// (not silently dropped).
struct TacticalMember {
    MemberId id;
    CatalogId templateId;
    std::uint32_t quantity = 0;
    std::uint32_t experience = 0;
    std::vector<UpgradeId> persistentUpgrades;
};

struct TacticalArmy {
    ArmyId id;
    FactionId factionId;
    std::uint8_t side = 0;                 // opposed tactical side index
    std::vector<TacticalMember> members;
};

struct TacticalSeat {
    PlayerId playerId;
    bool human = true;
};

struct BattleRequestV1 {
    std::uint32_t contractVersion = 1;
    BattleId id;
    std::string stateHash;                 // canonical state hash at dispatch
    PackId packId;
    PackHash packHash;
    ProfileId tacticalProfile;
    TacticalMapRef tacticalMap;
    std::uint64_t tacticalSeed = 0;
    RegionId regionId;
    std::vector<TacticalArmy> participants;
    std::vector<TacticalSeat> seats;
    std::uint32_t startingResources = 0;
    std::vector<std::string> persistentSciences;  // empty = not applicable
    std::vector<std::string> reinforcements;      // empty = not applicable
    std::vector<RegionId> strategicObjectives;    // empty = not applicable
    std::vector<MemberId> heroes;                 // empty = not applicable
    std::string requestChecksum;
};

std::string battleRequestChecksum(const BattleRequestV1& r);

// BattleResultV1: full native result (§6).
enum class CompletionReason : std::uint8_t { Victory = 1, Draw = 2, Forfeit = 3 };

struct SurvivingMember {
    MemberId id;
    std::uint32_t quantity = 0;
    std::uint32_t experience = 0;
    std::vector<UpgradeId> persistentUpgrades;
};

struct SurvivingArmy {
    ArmyId id;
    std::vector<SurvivingMember> members;
};

struct HeroRecord {
    MemberId id;
    bool survived = false;
    bool revived = false;
};

struct BattleResultV1 {
    std::uint32_t contractVersion = 1;
    BattleId id;
    std::string requestChecksum;
    CompletionReason completionReason = CompletionReason::Victory;
    std::optional<FactionId> victor;   // nullopt = draw / forfeit
    std::vector<SurvivingArmy> survivors;
    std::vector<HeroRecord> heroes;                   // empty = not applicable
    std::vector<std::string> persistentSciences;      // per-player, empty = n/a
    struct {
        std::vector<ArmyId> called;
        std::vector<ArmyId> uncalled;
    } reinforcements;
    std::vector<RegionId> destroyedObjectives;
    std::string tacticalStatistics;                   // empty = n/a
    std::optional<std::uint64_t> captureFrame;        // deterministic boundary when available
    std::string resultChecksum;
};

std::string battleResultChecksum(const BattleResultV1& r);

struct CampaignState {
    std::uint32_t schemaVersion = 1;
    PackId packId;
    PackHash packHash;
    ProfileId tacticalProfile;
    TacticalMapRef tacticalMap;
    std::uint64_t initialSeed = 0;
    std::uint32_t turn = 0;
    Phase phase = Phase::Planning;
    std::map<FactionId, StateFaction> factions;          // sorted by id
    std::map<RegionId, StateRegion> regions;             // sorted by id
    std::map<ArmyId, StateArmy> armies;                  // sorted by id
    std::set<ArmyId> movedThisTurn;                      // sorted
    std::optional<PendingBattle> pendingBattle;
    std::vector<CommandEnvelope> acceptedCommands;       // canonical resolution order
};

// Canonical, order-independent serialization of state. Bytes never depend on
// source property order or container iteration.
std::string canonicalState(const CampaignState& s);
// SHA-256 hex of canonicalState.
std::string stateHash(const CampaignState& s);

enum class ResolutionEventType : std::uint8_t { ArmyMoved = 1, BattlePending = 2 };

struct ResolutionEvent {
    ResolutionEventType type;
    ArmyId armyId;
    RegionId regionId;
    BattleId battleId;
};

struct ResolutionOutcome {
    CampaignState state;  // new state after resolution
    std::vector<ResolutionEvent> events;
    BattleRequestV1 request;  // meaningful only when battlePending is true
    bool battlePending = false;
};

// Build the initial state from a validated pack and an explicit seed.
WotrResult<CampaignState> initialState(const Pack& pack, std::uint64_t seed);

// Resolve a batch of command envelopes (WEGO): validation, canonical
// ordering, moves, conflict detection. All-or-nothing: on failure the
// input state is unchanged.
WotrResult<ResolutionOutcome> resolveCommands(CampaignState state,
                                              std::vector<CommandEnvelope> envelopes);

// Apply a battle result once, transactionally. A second apply, a wrong
// battle id, a mismatched request checksum or a damaged result checksum
// leaves the state byte-identical and returns a named error.
WotrResult<CampaignState> applyBattleResult(CampaignState state,
                                            const BattleResultV1& result);

} // namespace WotrRemaster
