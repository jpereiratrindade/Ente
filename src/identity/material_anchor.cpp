#include "ente/identity/material_anchor.hpp"
#include "ente/core/hash.hpp"
#include <format>

namespace ente::identity {

std::expected<MaterialBinding, core::EnteError> MaterialBindingRegistry::bind_initial_anchor(
    const core::IdentityId& id,
    MaterialAnchor anchor,
    core::LogicalTime time
) noexcept {
    if (!bindings_.empty()) {
        return std::unexpected(core::EnteError::GenesisAlreadyExists);
    }
    if (id.empty() || anchor.id.empty() || anchor.hardware_fingerprint.empty()) {
        return std::unexpected(core::EnteError::IdentityMismatch);
    }

    std::string payload = std::format("{}:{}:{}:{}",
        id.view(),
        anchor.id.view(),
        anchor.hardware_fingerprint,
        time
    );
    core::Digest b_digest = core::HashUtil::sha256(payload);

    MaterialBinding binding{
        .identity = id,
        .anchor = std::move(anchor),
        .bound_at = time,
        .previous_anchor = std::nullopt,
        .binding_digest = b_digest
    };

    bindings_.push_back(binding);
    return binding;
}

std::expected<MaterialBinding, core::EnteError> MaterialBindingRegistry::migrate_to_anchor(
    MaterialAnchor new_anchor,
    core::LogicalTime time
) noexcept {
    if (bindings_.empty()) {
        return std::unexpected(core::EnteError::GenesisNotEstablished);
    }
    if (new_anchor.id.empty() || new_anchor.hardware_fingerprint.empty()) {
        return std::unexpected(core::EnteError::IdentityMismatch);
    }
    if (time <= bindings_.back().bound_at) {
        return std::unexpected(core::EnteError::InvalidLogicalTime);
    }
    for (const auto& binding : bindings_) {
        if (binding.anchor.id == new_anchor.id) {
            return std::unexpected(core::EnteError::IdentityMismatch);
        }
    }

    const auto& prev = bindings_.back();
    std::string payload = std::format("{}:{}:{}:{}:{}",
        prev.identity.view(),
        new_anchor.id.view(),
        new_anchor.hardware_fingerprint,
        time,
        prev.binding_digest.value
    );
    core::Digest b_digest = core::HashUtil::sha256(payload);

    MaterialBinding binding{
        .identity = prev.identity,
        .anchor = std::move(new_anchor),
        .bound_at = time,
        .previous_anchor = prev.anchor.id,
        .binding_digest = b_digest
    };

    bindings_.push_back(binding);
    return binding;
}

} // namespace ente::identity
