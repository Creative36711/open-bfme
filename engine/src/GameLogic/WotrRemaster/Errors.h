// OpenBFME. GPL-3.0.
// WotrRemaster deterministic strategic core: named error codes.
// Every failure carries a stable code (for assertions and the report) plus a
// human-readable message with context (field path, id, ...). Codes never
// change once published.

#pragma once

#include <cstdint>
#include <string>

namespace WotrRemaster {

// Named, nonzero failure codes. Ok = 0; everything else is a failure.
enum class WotrErrc : std::uint16_t {
    Ok = 0,

    // --- pack loading and schema validation --------------------------------
    InvalidPack = 1,          // malformed or structurally invalid pack document
    DuplicateId = 2,          // region / faction / army / member id is not unique
    BrokenTopology = 3,       // region graph references unknown regions or is asymmetric
    MissingTacticalProfile = 4, // pack.tactical_profile missing or unsupported
    MissingTacticalMap = 5,   // pack.tactical_map missing or empty
    UnstableReference = 6,    // a reference cannot resolve deterministically (.. segments, ...)
    AbsoluteReference = 7,    // an absolute author-machine path reference
    ContentHashMismatch = 8,  // pack.contentHash does not match recomputed hash

    // --- commands and resolution ------------------------------------------
    UnknownEntity = 9,        // referenced army / member / region does not exist
    InvalidCommand = 10,      // command fails rules validation (adjacency, turn, ...)
    DamagedChecksum = 11,     // envelope/request/result checksum does not verify
    UnknownBattle = 12,       // battle id unknown or no pending battle to complete
    MismatchedChecksum = 13,  // result requestChecksum does not match the pending request
    DuplicateResult = 14,     // battle result already applied / battle has no pending request
    InvalidResult = 15,       // result fails contract validation (version, fields, ...)

    // --- save / load -------------------------------------------------------
    UnknownSaveVersion = 16,  // save envelope version not supported
    DamagedSaveChecksum = 17, // save envelope damaged or tampered
    IncompatiblePack = 18,    // save belongs to another pack / tactical profile
    InvalidSave = 19,         // save envelope or embedded state cannot be parsed
};

// Stable string name of a code (for reports and diagnostics).
const char* errcName(WotrErrc code) noexcept;

// Human-readable message for a code, with context appended by the caller.
std::string errcMessage(WotrErrc code) noexcept;

// Carried by helpers inside the loader and converted to a WotrResult at the
// loadPack boundary.
struct WotrError {
    WotrErrc code;
    std::string message;
};

// Result wrapper: value on success, named code + message on failure.
template <typename T>
struct WotrResult {
    T value{};
    WotrErrc err = WotrErrc::Ok;
    std::string message;

    [[nodiscard]] bool ok() const noexcept { return err == WotrErrc::Ok; }
};

template <typename T>
WotrResult<T> fail(WotrErrc code, std::string message) {
    WotrResult<T> r;
    r.err = code;
    r.message = std::move(message);
    return r;
}

template <typename T>
WotrResult<T> ok(T value) {
    WotrResult<T> r;
    r.value = std::move(value);
    return r;
}

} // namespace WotrRemaster
