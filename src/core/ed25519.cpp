#include "ente/core/ed25519.hpp"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <algorithm>
#include <charconv>

namespace ente::core {

namespace {

template <size_t N>
std::string encode_hex(const std::array<uint8_t, N>& bytes) {
    constexpr char digits[] = "0123456789abcdef";
    std::string output;
    output.resize(N * 2);
    for (size_t i = 0; i < N; ++i) {
        output[i * 2] = digits[bytes[i] >> 4];
        output[i * 2 + 1] = digits[bytes[i] & 0x0f];
    }
    return output;
}

template <size_t N>
bool decode_hex(std::string_view encoded, std::array<uint8_t, N>& output) noexcept {
    if (encoded.size() != N * 2) return false;
    for (size_t i = 0; i < N; ++i) {
        unsigned int value = 0;
        const char* begin = encoded.data() + i * 2;
        const auto [end, error] = std::from_chars(begin, begin + 2, value, 16);
        if (error != std::errc{} || end != begin + 2 || value > 0xff) return false;
        output[i] = static_cast<uint8_t>(value);
    }
    return true;
}

} // namespace

Ed25519KeyPair::Ed25519KeyPair(
    std::array<uint8_t, key_size> private_seed,
    std::array<uint8_t, key_size> public_key
) noexcept
    : private_seed_(private_seed)
    , public_key_(public_key) {}

Ed25519KeyPair::Ed25519KeyPair(Ed25519KeyPair&& other) noexcept
    : private_seed_(other.private_seed_)
    , public_key_(other.public_key_) {
    other.clear_private_seed();
    other.public_key_.fill(0);
}

Ed25519KeyPair& Ed25519KeyPair::operator=(Ed25519KeyPair&& other) noexcept {
    if (this == &other) return *this;
    clear_private_seed();
    private_seed_ = other.private_seed_;
    public_key_ = other.public_key_;
    other.clear_private_seed();
    other.public_key_.fill(0);
    return *this;
}

Ed25519KeyPair::~Ed25519KeyPair() {
    clear_private_seed();
}

void Ed25519KeyPair::clear_private_seed() noexcept {
    OPENSSL_cleanse(private_seed_.data(), private_seed_.size());
}

std::expected<Ed25519KeyPair, EnteError> Ed25519KeyPair::generate() noexcept {
    std::array<uint8_t, key_size> seed{};
    if (RAND_bytes(seed.data(), static_cast<int>(seed.size())) != 1) {
        return std::unexpected(EnteError::CryptographicFailure);
    }
    auto result = from_private_seed(seed);
    OPENSSL_cleanse(seed.data(), seed.size());
    return result;
}

std::expected<Ed25519KeyPair, EnteError> Ed25519KeyPair::from_private_seed(
    std::span<const uint8_t, key_size> seed
) noexcept {
    EVP_PKEY* key = EVP_PKEY_new_raw_private_key_ex(
        nullptr,
        "ED25519",
        nullptr,
        seed.data(),
        seed.size()
    );
    if (key == nullptr) return std::unexpected(EnteError::CryptographicFailure);

    std::array<uint8_t, key_size> public_key{};
    size_t public_key_size = public_key.size();
    const bool derived = EVP_PKEY_get_raw_public_key(
        key,
        public_key.data(),
        &public_key_size
    ) == 1 && public_key_size == public_key.size();
    EVP_PKEY_free(key);
    if (!derived) return std::unexpected(EnteError::CryptographicFailure);

    std::array<uint8_t, key_size> private_seed{};
    std::copy(seed.begin(), seed.end(), private_seed.begin());
    Ed25519KeyPair result(private_seed, public_key);
    OPENSSL_cleanse(private_seed.data(), private_seed.size());
    return result;
}

std::expected<std::string, EnteError> Ed25519KeyPair::sign_hex(
    std::string_view message
) const noexcept {
    EVP_PKEY* key = EVP_PKEY_new_raw_private_key_ex(
        nullptr,
        "ED25519",
        nullptr,
        private_seed_.data(),
        private_seed_.size()
    );
    if (key == nullptr) return std::unexpected(EnteError::CryptographicFailure);

    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (context == nullptr) {
        EVP_PKEY_free(key);
        return std::unexpected(EnteError::CryptographicFailure);
    }

    std::array<uint8_t, signature_size> signature{};
    size_t signature_length = signature.size();
    const bool signed_ok = EVP_DigestSignInit(context, nullptr, nullptr, nullptr, key) == 1 &&
        EVP_DigestSign(
            context,
            signature.data(),
            &signature_length,
            reinterpret_cast<const uint8_t*>(message.data()),
            message.size()
        ) == 1 && signature_length == signature.size();

    EVP_MD_CTX_free(context);
    EVP_PKEY_free(key);
    if (!signed_ok) return std::unexpected(EnteError::CryptographicFailure);
    return encode_hex(signature);
}

std::string Ed25519KeyPair::public_key_hex() const {
    return encode_hex(public_key_);
}

bool Ed25519KeyPair::verify_hex(
    std::string_view public_key_hex,
    std::string_view message,
    std::string_view signature_hex
) noexcept {
    std::array<uint8_t, key_size> public_key{};
    std::array<uint8_t, signature_size> signature{};
    if (!decode_hex(public_key_hex, public_key) ||
        !decode_hex(signature_hex, signature)) {
        return false;
    }

    EVP_PKEY* key = EVP_PKEY_new_raw_public_key_ex(
        nullptr,
        "ED25519",
        nullptr,
        public_key.data(),
        public_key.size()
    );
    if (key == nullptr) return false;
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (context == nullptr) {
        EVP_PKEY_free(key);
        return false;
    }

    const bool verified = EVP_DigestVerifyInit(context, nullptr, nullptr, nullptr, key) == 1 &&
        EVP_DigestVerify(
            context,
            signature.data(),
            signature.size(),
            reinterpret_cast<const uint8_t*>(message.data()),
            message.size()
        ) == 1;
    EVP_MD_CTX_free(context);
    EVP_PKEY_free(key);
    return verified;
}

} // namespace ente::core
