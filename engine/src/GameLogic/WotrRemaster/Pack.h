// OpenBFME. GPL-3.0.
// WotrRemaster deterministic strategic core: Campaign Pack manifest, model
// and loader. A pack is versioned, synthetic data (no retail bytes):
// manifest, regions/factions/armies with stable ids, topology and a single
// tactical map reference. The loader enforces the schema semantics:
// unique ids, valid topology, a known tactical profile and map, and only
// stable references (no absolute or author-machine paths).

#pragma once

#include "Errors.h"
#include "Hash.h"
#include "Json.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace WotrRemaster {

// A single localized label is always a catalog id; English is the source
// per-message fallback (core never touches locale).
using CatalogId = std::string;
using PackId = std::string;
using PackHash = std::string;   // lowercase hex sha-256
using ProfileId = std::string;  // "vanilla_rotwk_201"
using TacticalMapRef = std::string;
using BattleId = std::string;
using PlayerId = std::string;
using CommandId = std::string;
using FactionId = std::string;
using RegionId = std::string;
using ArmyId = std::string;
using MemberId = std::string;
using UpgradeId = std::string;

// The only tactical profile this core phase supports. Other profiles are
// rejected by the loader (unknown profile), never silently adopted.
inline const char* kVanillaRotWk201Profile = "vanilla_rotwk_201";

struct PackMember {
    MemberId id;
    CatalogId kind;              // unit template catalog id
    std::uint32_t quantity = 0;
    std::uint32_t experience = 0;
    std::vector<UpgradeId> persistentUpgrades;
};

struct PackArmy {
    ArmyId id;
    FactionId factionId;
    RegionId regionId;
    std::vector<PackMember> members;
};

struct PackRegion {
    RegionId id;
    CatalogId name;
    std::vector<RegionId> connections; // as declared in the pack (loader checks symmetry)
};

struct PackFaction {
    FactionId id;
    CatalogId name;
};

struct PackManifest {
    PackId id;
    std::uint32_t version = 1;
    ProfileId tacticalProfile;
    TacticalMapRef tacticalMap;
    std::optional<PackHash> contentHash; // sha-256 of the pack without this field
};

struct Pack {
    PackManifest manifest;
    std::vector<PackRegion> regions;
    std::vector<PackFaction> factions;
    std::vector<PackArmy> armies;
};

// Load and fully validate a pack document. All-or-nothing: on failure the
// returned error carries a named code and a context-rich message.
WotrResult<Pack> loadPack(std::string jsonText);

// Recompute the pack content hash: sha-256 over the canonical serialization
// of the pack with manifest.contentHash omitted (self-excluding).
PackHash contentHashOf(const Pack& pack);

// Canonical serialization of a pack (contentHash field omitted).
std::string canonicalPack(const Pack& pack);

// Validate a bare stable id (non-empty, no path separators or control chars).
bool validId(const std::string& id);

// Is a reference string stable across machines? pack:/retail:/custom: and
// bare ids are stable; absolute paths and .. segments are not.
bool referenceIsStable(const std::string& ref);

} // namespace WotrRemaster
