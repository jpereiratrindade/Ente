#include "ente/authority/authority.hpp"
#include "ente/core/hash.hpp"
#include <format>

namespace ente::authority {

namespace {

void append_field(std::string& output, std::string_view value) {
    output += std::to_string(value.size());
    output += ':';
    output.append(value);
}

bool valid_public_key(std::string_view value) noexcept {
    if (value.size() != core::Ed25519KeyPair::key_size * 2) return false;
    for (const char c : value) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}

std::string root_epoch_material(
    const AuthorityEpochId& epoch,
    const AuthorityId& authority,
    std::string_view public_key,
    core::LogicalTime time
) {
    std::string result{"ENTE_AUTHORITY_ROOT_V1"};
    append_field(result, epoch.view());
    append_field(result, authority.view());
    append_field(result, public_key);
    append_field(result, std::to_string(time));
    return result;
}

std::string delegation_material(
    const AuthorityEpochId& new_epoch,
    const AuthorityEpoch& predecessor,
    const AuthorityId& new_authority,
    std::string_view new_public_key,
    core::LogicalTime time
) {
    std::string result{"ENTE_AUTHORITY_DELEGATION_V1"};
    append_field(result, new_epoch.view());
    append_field(result, predecessor.epoch_id.view());
    append_field(result, new_authority.view());
    append_field(result, new_public_key);
    append_field(result, std::to_string(time));
    append_field(result, predecessor.epoch_digest.value);
    append_field(result, "STRICT_LINEAGE");
    return result;
}

core::Digest delegated_epoch_digest(
    std::string_view delegation,
    std::string_view signature
) {
    std::string material{"ENTE_AUTHORITY_EPOCH_V1"};
    append_field(material, delegation);
    append_field(material, signature);
    return core::HashUtil::sha256(material);
}

} // namespace

std::expected<AuthorityEpoch, core::EnteError> AuthorityLineage::initialize_root_epoch(
    const AuthorityId& root_auth,
    std::string root_public_key,
    core::LogicalTime time
) noexcept {
    if (!epochs_.empty()) {
        return std::unexpected(core::EnteError::GenesisAlreadyExists);
    }
    if (root_auth.empty() || !valid_public_key(root_public_key)) {
        return std::unexpected(core::EnteError::IdentityMismatch);
    }

    AuthorityEpochId ep_id("epoch-0");
    core::Digest ep_digest = core::HashUtil::sha256(
        root_epoch_material(ep_id, root_auth, root_public_key, time));

    AuthorityEpoch ep{
        .epoch_id = ep_id,
        .predecessor_epoch = std::nullopt,
        .authorized_authority = root_auth,
        .authority_public_key = std::move(root_public_key),
        .activation_time = time,
        .epoch_digest = ep_digest,
        .delegation_signature = {},
        .status = EpochStatus::Active
    };

    epochs_.push_back(ep);
    return ep;
}

std::expected<AuthorityEpoch, core::EnteError> AuthorityLineage::transition_epoch(
    const AuthorityId& new_auth,
    std::string new_public_key,
    core::LogicalTime time,
    std::string delegation_signature
) noexcept {
    if (epochs_.empty()) {
        return std::unexpected(core::EnteError::GenesisNotEstablished);
    }
    if (new_auth.empty() || !valid_public_key(new_public_key)) {
        return std::unexpected(core::EnteError::IdentityMismatch);
    }
    if (time <= epochs_.back().activation_time) {
        return std::unexpected(core::EnteError::InvalidLogicalTime);
    }

    AuthorityEpochId new_ep_id(std::format("epoch-{}", epochs_.size()));
    const auto& prev_ep = epochs_.back();
    const std::string delegation = delegation_material(
        new_ep_id, prev_ep, new_auth, new_public_key, time);
    if (!core::Ed25519KeyPair::verify_hex(
            prev_ep.authority_public_key, delegation, delegation_signature)) {
        return std::unexpected(core::EnteError::InvalidSignature);
    }
    core::Digest ep_digest = delegated_epoch_digest(delegation, delegation_signature);

    // No state changes before every validation above has succeeded.
    epochs_.back().status = EpochStatus::Retired;

    AuthorityEpoch ep{
        .epoch_id = new_ep_id,
        .predecessor_epoch = prev_ep.epoch_id,
        .authorized_authority = new_auth,
        .authority_public_key = std::move(new_public_key),
        .activation_time = time,
        .epoch_digest = ep_digest,
        .delegation_signature = std::move(delegation_signature),
        .status = EpochStatus::Active
    };

    epochs_.push_back(ep);
    return ep;
}

std::string AuthorityLineage::delegation_message(
    const AuthorityId& new_auth,
    std::string_view new_public_key,
    core::LogicalTime time
) const {
    if (epochs_.empty()) return {};
    return delegation_material(
        AuthorityEpochId(std::format("epoch-{}", epochs_.size())),
        epochs_.back(),
        new_auth,
        new_public_key,
        time
    );
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

    for (size_t i = 0; i < epochs_.size(); ++i) {
        const auto& ep = epochs_[i];
        if (i == 0) {
            if (ep.predecessor_epoch.has_value() || !ep.delegation_signature.empty() ||
                !valid_public_key(ep.authority_public_key)) return false;
            if (core::HashUtil::sha256(root_epoch_material(
                    ep.epoch_id, ep.authorized_authority,
                    ep.authority_public_key, ep.activation_time)) != ep.epoch_digest) return false;
        } else {
            if (ep.activation_time <= epochs_[i - 1].activation_time) return false;
            if (!ep.predecessor_epoch.has_value() || *ep.predecessor_epoch != epochs_[i - 1].epoch_id) return false;
            if (!valid_public_key(ep.authority_public_key)) return false;
            const std::string delegation = delegation_material(
                ep.epoch_id, epochs_[i - 1], ep.authorized_authority,
                ep.authority_public_key, ep.activation_time);
            if (!core::Ed25519KeyPair::verify_hex(
                    epochs_[i - 1].authority_public_key,
                    delegation,
                    ep.delegation_signature)) return false;
            if (delegated_epoch_digest(delegation, ep.delegation_signature) != ep.epoch_digest) return false;
        }
        const auto expected_status = i + 1 == epochs_.size()
            ? EpochStatus::Active
            : EpochStatus::Retired;
        if (ep.status != expected_status) return false;
    }

    return true;
}

} // namespace ente::authority
