#pragma once

#include "ente/core/types.hpp"

#include <array>
#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace ente::core {

class Ed25519KeyPair {
public:
    static constexpr size_t key_size = 32;
    static constexpr size_t signature_size = 64;

    Ed25519KeyPair(const Ed25519KeyPair&) = delete;
    Ed25519KeyPair& operator=(const Ed25519KeyPair&) = delete;
    Ed25519KeyPair(Ed25519KeyPair&& other) noexcept;
    Ed25519KeyPair& operator=(Ed25519KeyPair&& other) noexcept;
    ~Ed25519KeyPair();

    [[nodiscard]] static std::expected<Ed25519KeyPair, EnteError> generate() noexcept;
    [[nodiscard]] static std::expected<Ed25519KeyPair, EnteError> from_private_seed(
        std::span<const uint8_t, key_size> seed
    ) noexcept;

    [[nodiscard]] std::expected<std::string, EnteError> sign_hex(
        std::string_view message
    ) const noexcept;
    [[nodiscard]] std::string public_key_hex() const;

    [[nodiscard]] static bool verify_hex(
        std::string_view public_key_hex,
        std::string_view message,
        std::string_view signature_hex
    ) noexcept;

private:
    Ed25519KeyPair(
        std::array<uint8_t, key_size> private_seed,
        std::array<uint8_t, key_size> public_key
    ) noexcept;

    void clear_private_seed() noexcept;

    std::array<uint8_t, key_size> private_seed_{};
    std::array<uint8_t, key_size> public_key_{};
};

} // namespace ente::core
