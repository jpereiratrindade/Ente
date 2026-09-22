#include "ente/core/prng.hpp"
#include "ente/core/hash.hpp"
#include <format>
#include <cstring>

namespace ente::core {

uint64_t EventScopedPRNG::derive_u64(
    const EventId& event_id,
    std::string_view purpose,
    uint64_t invocation_index
) const noexcept {
    std::string payload = std::format("{}:{}:{}:{}",
        global_seed_,
        event_id.view(),
        purpose,
        invocation_index
    );

    Digest hash = HashUtil::sha256(payload);

    // Extract first 8 bytes of hash as uint64_t
    uint64_t result = 0;
    for (size_t i = 0; i < 16; ++i) {
        char c = hash.value[i];
        uint8_t nibble = 0;
        if (c >= '0' && c <= '9') nibble = static_cast<uint8_t>(c - '0');
        else if (c >= 'a' && c <= 'f') nibble = static_cast<uint8_t>(10 + (c - 'a'));
        else if (c >= 'A' && c <= 'F') nibble = static_cast<uint8_t>(10 + (c - 'A'));
        result = (result << 4) | nibble;
    }

    return result;
}

double EventScopedPRNG::derive_double(
    const EventId& event_id,
    std::string_view purpose,
    uint64_t invocation_index
) const noexcept {
    uint64_t val = derive_u64(event_id, purpose, invocation_index);
    // Cast to double and divide by 2^64 to strictly guarantee [0.0, 1.0)
    return static_cast<double>(val) / (static_cast<double>(UINT64_MAX) + 1.0);
}

} // namespace ente::core
