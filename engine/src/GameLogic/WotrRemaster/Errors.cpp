// OpenBFME. GPL-3.0.

#include "Errors.h"

namespace WotrRemaster {

const char* errcName(WotrErrc code) noexcept {
    switch (code) {
        case WotrErrc::Ok: return "OK";
        case WotrErrc::InvalidPack: return "ERR_INVALID_PACK";
        case WotrErrc::DuplicateId: return "ERR_DUPLICATE_ID";
        case WotrErrc::BrokenTopology: return "ERR_BROKEN_TOPOLOGY";
        case WotrErrc::MissingTacticalProfile: return "ERR_MISSING_TACTICAL_PROFILE";
        case WotrErrc::MissingTacticalMap: return "ERR_MISSING_TACTICAL_MAP";
        case WotrErrc::UnstableReference: return "ERR_UNSTABLE_REFERENCE";
        case WotrErrc::AbsoluteReference: return "ERR_ABSOLUTE_REFERENCE";
        case WotrErrc::ContentHashMismatch: return "ERR_CONTENT_HASH_MISMATCH";
        case WotrErrc::UnknownEntity: return "ERR_UNKNOWN_ENTITY";
        case WotrErrc::InvalidCommand: return "ERR_INVALID_COMMAND";
        case WotrErrc::DamagedChecksum: return "ERR_DAMAGED_CHECKSUM";
        case WotrErrc::UnknownBattle: return "ERR_UNKNOWN_BATTLE";
        case WotrErrc::MismatchedChecksum: return "ERR_MISMATCHED_CHECKSUM";
        case WotrErrc::DuplicateResult: return "ERR_DUPLICATE_RESULT";
        case WotrErrc::InvalidResult: return "ERR_INVALID_RESULT";
        case WotrErrc::UnknownSaveVersion: return "ERR_UNKNOWN_SAVE_VERSION";
        case WotrErrc::DamagedSaveChecksum: return "ERR_DAMAGED_SAVE_CHECKSUM";
        case WotrErrc::IncompatiblePack: return "ERR_INCOMPATIBLE_PACK";
        case WotrErrc::InvalidSave: return "ERR_INVALID_SAVE";
        default: return "ERR_UNKNOWN";
    }
}

std::string errcMessage(WotrErrc code) noexcept {
    switch (code) {
        case WotrErrc::Ok: return "no error";
        case WotrErrc::InvalidPack: return "invalid campaign pack document";
        case WotrErrc::DuplicateId: return "duplicate stable id in campaign pack";
        case WotrErrc::BrokenTopology: return "broken region topology";
        case WotrErrc::MissingTacticalProfile: return "missing or unsupported tactical profile";
        case WotrErrc::MissingTacticalMap: return "missing tactical map reference";
        case WotrErrc::UnstableReference: return "unstable (non-deterministic) reference";
        case WotrErrc::AbsoluteReference: return "absolute path reference is not allowed";
        case WotrErrc::ContentHashMismatch: return "pack content hash mismatch";
        case WotrErrc::UnknownEntity: return "unknown entity referenced";
        case WotrErrc::InvalidCommand: return "invalid command";
        case WotrErrc::DamagedChecksum: return "damaged checksum";
        case WotrErrc::UnknownBattle: return "unknown battle or no pending battle";
        case WotrErrc::MismatchedChecksum: return "result checksum does not match the request";
        case WotrErrc::DuplicateResult: return "duplicate battle result";
        case WotrErrc::InvalidResult: return "invalid battle result";
        case WotrErrc::UnknownSaveVersion: return "unknown save envelope version";
        case WotrErrc::DamagedSaveChecksum: return "damaged save envelope checksum";
        case WotrErrc::IncompatiblePack: return "save is incompatible with the campaign pack";
        case WotrErrc::InvalidSave: return "invalid save envelope";
        default: return "unknown error";
    }
}

} // namespace WotrRemaster
