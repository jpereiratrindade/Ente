#include "ente/authority/authority.hpp"
#include "ente/core/hash.hpp"
#include <format>

namespace ente::authority {

std::expected<AuthorityEpoch, core::EnteError> AuthorityLineage::initialize_root_epoch(
    const AuthorityId& root_auth,
    core::LogicalTime time
) noexcept {
    if (!epochs_.empty()) {
        return std::unexpected(core::EnteError::GenesisAlreadyExists);
    }
    if (root_auth.empty()) {
        return std::unexpected(core::EnteError::IdentityMismatch);
    }

    AuthorityEpochId ep_id("epoch-0");
    std::string payload = std::format("{}:{}:{}", ep_id.view(), root_auth.view(), time);
    core::Digest ep_digest = core::HashUtil::sha256(payload);

    AuthorityEpoch ep{
        .epoch_id = ep_id,
        .predecessor_epoch = std::nullopt,
        .authorized_authority = root_auth,
        .activation_time = time,
        .epoch_digest = ep_digest,
        .status = EpochStatus::Active
    };

    epochs_.push_back(ep);
    return ep;
}

std::expected<AuthorityEpoch, core::EnteError> AuthorityLineage::transition_epoch(
    const AuthorityId& new_auth,
    core::LogicalTime time
) noexcept {
    if (epochs_.empty()) {
        return std::unexpected(core::EnteError::GenesisNotEstablished);
    }
    if (new_auth.empty()) {
        return std::unexpected(core::EnteError::IdentityMismatch);
    }
    if (time <= epochs_.back().activation_time) {
        return std::unexpected(core::EnteError::InvalidLogicalTime);
    }

    // Retire previous epoch
    epochs_.back().status = EpochStatus::Retired;

    AuthorityEpochId new_ep_id(std::format("epoch-{}", epochs_.size()));
    const auto& prev_ep = epochs_.back();

    std::string payload = std::format("{}:{}:{}:{}:{}",
        new_ep_id.view(),
        prev_ep.epoch_id.view(),
        new_auth.view(),
        time,
        prev_ep.epoch_digest.value
    );
    core::Digest ep_digest = core::HashUtil::sha256(payload);

    AuthorityEpoch ep{
        .epoch_id = new_ep_id,
        .predecessor_epoch = prev_ep.epoch_id,
        .authorized_authority = new_auth,
        .activation_time = time,
        .epoch_digest = ep_digest,
        .status = EpochStatus::Active
    };

    epochs_.push_back(ep);
    return ep;
}

bool AuthorityLineage::is_authority_authorized(const AuthorityId& auth, const AuthorityEpochId& epoch) const noexcept {
    for (const auto& ep : epochs_) {
        if (ep.epoch_id == epoch && ep.authorized_authority == auth && ep.status == EpochStatus::Active) {
            return true;
        }
    }
    return false;
}

bool AuthorityLineage::is_epoch_legitimate(const AuthorityId& auth, const AuthorityEpochId& epoch) const noexcept {
    for (const auto& ep : epochs_) {
        if (ep.epoch_id == epoch && ep.authorized_authority == auth && ep.status != EpochStatus::Invalid) {
            return true;
        }
    }
    return false;
}

bool AuthorityLineage::is_epoch_legitimate_at(
    const AuthorityId& auth,
    const AuthorityEpochId& epoch,
    core::LogicalTime event_time
) const noexcept {
    for (size_t i = 0; i < epochs_.size(); ++i) {
        const auto& candidate = epochs_[i];
        if (candidate.epoch_id != epoch || candidate.authorized_authority != auth ||
            candidate.status == EpochStatus::Invalid || event_time < candidate.activation_time) {
            continue;
        }
        if (i + 1 < epochs_.size() && event_time >= epochs_[i + 1].activation_time) {
            return false;
        }
        return true;
    }
    return false;
}

bool AuthorityLineage::verify_lineage_integrity() const noexcept {
    if (epochs_.empty()) {
        return false;
    }

    core::Digest prev_digest;
    for (size_t i = 0; i < epochs_.size(); ++i) {
        const auto& ep = epochs_[i];
        if (i == 0) {
            if (ep.predecessor_epoch.has_value()) return false;
            std::string payload = std::format("{}:{}:{}", ep.epoch_id.view(), ep.authorized_authority.view(), ep.activation_time);
            if (core::HashUtil::sha256(payload) != ep.epoch_digest) return false;
        } else {
            if (ep.activation_time <= epochs_[i - 1].activation_time) return false;
            if (!ep.predecessor_epoch.has_value() || *ep.predecessor_epoch != epochs_[i - 1].epoch_id) return false;
            std::string payload = std::format("{}:{}:{}:{}:{}",
                ep.epoch_id.view(),
                epochs_[i - 1].epoch_id.view(),
                ep.authorized_authority.view(),
                ep.activation_time,
                prev_digest.value
            );
            if (core::HashUtil::sha256(payload) != ep.epoch_digest) return false;
        }
        const auto expected_status = i + 1 == epochs_.size()
            ? EpochStatus::Active
            : EpochStatus::Retired;
        if (ep.status != expected_status) return false;
        prev_digest = ep.epoch_digest;
    }

    return true;
}

} // namespace ente::authority
