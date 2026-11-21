// OpenBFME. GPL-3.0.
// WotrRemaster deterministic strategic core: SHA-256.
// Self-contained implementation (no third-party dependency) so the core stays
// buildable outside the full engine CMake graph. All hashes used by the core
// (pack content hash, state hash, envelope/request/result checksums) are
// SHA-256 over canonical byte sequences.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace WotrRemaster {

// SHA-256 streaming digest, FIPS 180-4.
class Sha256 {
public:
    Sha256();
    void update(const void* data, std::size_t len);
    void update(const std::string& s);
    // Produce the 32-byte raw digest and reset the context.
    void finalize(std::uint8_t out[32]);

    // One-shot helpers: return the digest as a lowercase hex string.
    static std::string hashHex(const void* data, std::size_t len);
    static std::string hashHex(const std::string& s);

private:
    void transformBlock(const std::uint8_t block[64]);

    std::uint32_t h[8];
    std::uint8_t buffer[64];
    std::size_t bufferLen;
    std::uint64_t totalLen;
};

} // namespace WotrRemaster
