#pragma once

#include "ente/core/types.hpp"
#include <string>

namespace ente::identity {

enum class LifecycleStatus : uint8_t {
    PreGenesis,
    LifeActive,
    ConstitutiveSuspension,
    ContinuityUnresolved,
    DeathFinalized
};

[[nodiscard]] constexpr std::string_view to_string(LifecycleStatus s) noexcept {
    switch (s) {
        case LifecycleStatus::PreGenesis: return "PRE_GENESIS";
        case LifecycleStatus::LifeActive: return "LIFE_ACTIVE";
        case LifecycleStatus::ConstitutiveSuspension: return "CONSTITUTIVE_SUSPENSION";
        case LifecycleStatus::ContinuityUnresolved: return "CONTINUITY_UNRESOLVED";
        case LifecycleStatus::DeathFinalized: return "DEATH_FINALIZED";
    }
    return "UNKNOWN";
}

struct GenesisRecord {
    core::GenesisId id;
    core::IdentityId identity;
    core::Digest constitution_digest;
    core::Digest basal_state_digest;
    core::LogicalTime logical_time{0};
    core::Digest genesis_digest;
};

struct IdentityState {
    core::IdentityId id;
    core::GenesisId genesis;
    LifecycleStatus lifecycle{LifecycleStatus::PreGenesis};
    core::LogicalTime current_time{0};
};

} // namespace ente::identity
