// OpenBFME. GPL-3.0.
// WotrRemaster deterministic strategic core: versioned save envelope
// (WotrSaveEnvelopeV1, §9 of the architecture).
//
// Envelope: schema version, contract version, Campaign Pack identity/hash,
// tactical profile, canonical state bytes, canonical state hash, command
// cursor, RNG metadata and checksum. Load is transactional: parse and
// validate into a fresh state, require stateHash == stateHash(parsedState),
// swap only on success. Unknown version and damaged checksum are named,
// nonzero failures.

#pragma once

#include "Errors.h"
#include "Hash.h"
#include "Pack.h"
#include "State.h"

#include <cstdint>
#include <string>

namespace WotrRemaster {

struct WotrSaveEnvelopeV1 {
    std::uint32_t envelopeVersion = 1;  // save format version
    std::uint32_t contractVersion = 1;  // core contract version
    PackId packId;
    PackHash packHash;
    ProfileId tacticalProfile;
    std::string stateBytes;     // canonical state bytes
    std::string stateHash;      // sha-256 hex of canonical state bytes
    std::uint64_t commandCursor = 0; // number of accepted commands
    std::string rngMetadata;    // "{}" — no ambient RNG in this core phase
    std::string checksum;       // sha-256 hex of the envelope fields above
};

// Canonical serialization of the envelope fields excluding checksum.
std::string saveEnvelopePayload(const WotrSaveEnvelopeV1& e);
// Full canonical serialization including the checksum field.
std::string serializeSaveEnvelope(const WotrSaveEnvelopeV1& e);

// Save a campaign state into envelope text (ready to write to disk).
std::string saveCampaign(const CampaignState& s, const Pack& pack);

// Parse + structurally validate an envelope text. Named errors:
// ERR_UNKNOWN_SAVE_VERSION, ERR_DAMAGED_SAVE_CHECKSUM, ERR_INVALID_SAVE.
WotrResult<WotrSaveEnvelopeV1> parseSaveEnvelope(const std::string& text);

// Load a campaign from envelope text into a fresh instance, transactionally.
// Named errors include ERR_INCOMPATIBLE_PACK (pack/profile mismatch),
// ERR_DAMAGED_CHECKSUM (embedded acceptedCommands[*].checksum mismatch),
// ERR_DAMAGED_SAVE_CHECKSUM (stateHash mismatch / non-canonical stateBytes)
// and errors from state validation (ERR_INVALID_SAVE). On failure no state is
// produced.
WotrResult<CampaignState> loadCampaign(const std::string& text, const Pack& pack);

} // namespace WotrRemaster
