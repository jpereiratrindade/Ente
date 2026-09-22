#pragma once

#include "ente/core/types.hpp"
#include <string_view>
#include <cstdint>

namespace ente::core {

// Event-Scoped Deterministic PRNG
// Generates reproducible pseudorandom values derived deterministically from (Seed, EventId, Purpose)
class EventScopedPRNG {
public:
    explicit EventScopedPRNG(std::string_view global_seed) : global_seed_(global_seed) {}

    // Derive a deterministic uint64_t for a specific event and purpose
    [[nodiscard]] uint64_t derive_u64(
        const EventId& event_id,
        std::string_view purpose,
        uint64_t invocation_index = 0
    ) const noexcept;

    // Derive a deterministic float in [0.0, 1.0)
    [[nodiscard]] double derive_double(
        const EventId& event_id,
        std::string_view purpose,
        uint64_t invocation_index = 0
    ) const noexcept;

private:
    std::string global_seed_;
};

} // namespace ente::core
