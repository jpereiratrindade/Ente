#pragma once

#include "ente/core/types.hpp"
#include <string_view>
#include <span>

namespace ente::core {

// Deterministic Cryptographic Hash function (SHA-256 implementation)
class HashUtil {
public:
    [[nodiscard]] static Digest sha256(std::string_view input) noexcept;
    [[nodiscard]] static Digest sha256(std::span<const uint8_t> data) noexcept;

    // Combine multiple digests or data streams deterministically
    [[nodiscard]] static Digest combine(const Digest& a, const Digest& b) noexcept;
    [[nodiscard]] static Digest combine(const Digest& a, std::string_view payload) noexcept;
};

} // namespace ente::core
