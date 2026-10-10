// sha256.hpp - a small, dependency-free SHA-256 (FIPS 180-4) implementation.
//
// Verified against the NIST / FIPS 180-4 test vectors in tests/test_sha256.cpp.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace wms {

class Sha256 {
public:
    using Digest = std::array<std::uint8_t, 32>;

    Sha256() noexcept;

    void update(const std::uint8_t* data, std::size_t length) noexcept;
    void update(std::string_view data) noexcept;

    // Pads the message and returns the digest. The object must not be updated
    // again afterwards; create a new one for the next message.
    Digest finish() noexcept;

    // One-shot helpers.
    static Digest digest(std::string_view data) noexcept;
    static std::string hexDigest(std::string_view data);

private:
    void transform(const std::uint8_t* block) noexcept;

    std::array<std::uint32_t, 8> state_;
    std::array<std::uint8_t, 64> buffer_;
    std::size_t bufferLength_ = 0;
    std::uint64_t totalBits_ = 0;
};

// Lower-case hex encoding of arbitrary bytes.
std::string toHex(const std::uint8_t* data, std::size_t length);

}  // namespace wms
