#pragma once

#include "ente/core/types.hpp"
#include "ente/core/ed25519.hpp"
#include <vector>
#include <string>
#include <optional>

namespace ente::authority {

struct AuthorityTag {};
using AuthorityId = core::StrongId<AuthorityTag>;

struct AuthorityEpochTag {};
using AuthorityEpochId = core::StrongId<AuthorityEpochTag>;

enum class EpochStatus : uint8_t {
    Active,
    Transitioning,
    Retired,
    Invalid
};

[[nodiscard]] constexpr std::string_view to_string(EpochStatus s) noexcept {
    switch (s) {
        case EpochStatus::Active: return "ACTIVE";
        case EpochStatus::Transitioning: return "TRANSITIONING";
        case EpochStatus::Retired: return "RETIRED";
        case EpochStatus::Invalid: return "INVALID";
    }
    return "UNKNOWN_STATUS";
}

struct AuthorityEpoch {
    AuthorityEpochId epoch_id;
    std::optional<AuthorityEpochId> predecessor_epoch;
    AuthorityId authorized_authority;
    std::string authority_public_key;
    core::LogicalTime activation_time{0};
    core::Digest epoch_digest;
    std::string delegation_signature;
    EpochStatus status{EpochStatus::Active};
};

class AuthorityLineage {
public:
    AuthorityLineage() = default;

    // Initialize root epoch at Genesis (C14)
    [[nodiscard]] std::expected<AuthorityEpoch, core::EnteError> initialize_root_epoch(
        const AuthorityId& root_auth,
        std::string root_public_key,
        core::LogicalTime time = 0
    ) noexcept;

    // Transition to new authority epoch under legitimate lineage (C14)
    [[nodiscard]] std::expected<AuthorityEpoch, core::EnteError> transition_epoch(
        const AuthorityId& new_auth,
        std::string new_public_key,
        core::LogicalTime time,
        std::string delegation_signature
    ) noexcept;

    [[nodiscard]] std::string delegation_message(
        const AuthorityId& new_auth,
        std::string_view new_public_key,
        core::LogicalTime time
    ) const;

    [[nodiscard]] bool is_authority_authorized(const AuthorityId& auth, const AuthorityEpochId& epoch) const noexcept;
    [[nodiscard]] bool is_epoch_legitimate(const AuthorityId& auth, const AuthorityEpochId& epoch) const noexcept;
    [[nodiscard]] bool is_epoch_legitimate_at(
        const AuthorityId& auth,
        const AuthorityEpochId& epoch,
        core::LogicalTime event_time
    ) const noexcept;
    [[nodiscard]] bool verify_lineage_integrity() const noexcept;
    [[nodiscard]] const std::vector<AuthorityEpoch>& epochs() const noexcept { return epochs_; }
    [[nodiscard]] const AuthorityEpoch& active_epoch() const { return epochs_.back(); }
    [[nodiscard]] bool empty() const noexcept { return epochs_.empty(); }

private:
    std::vector<AuthorityEpoch> epochs_;
};

} // namespace ente::authority
