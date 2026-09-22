#include "ente/identity/genesis_service.hpp"
#include "ente/core/hash.hpp"
#include <format>

namespace ente::identity {

std::expected<GenesisRecord, core::EnteError> GenesisService::create_genesis(const GenesisRequest& req) noexcept {
    if (record_.has_value()) {
        return std::unexpected(core::EnteError::GenesisAlreadyExists);
    }

    if (req.identity.empty()) {
        return std::unexpected(core::EnteError::IdentityMismatch);
    }

    core::GenesisId gen_id(std::format("gen-{}", req.identity.view()));
    
    // Compute genesis digest
    std::string payload = std::format("{}:{}:{}:{}",
        gen_id.view(),
        req.identity.view(),
        req.constitution_digest.value,
        req.basal_state_digest.value
    );
    core::Digest gen_digest = core::HashUtil::sha256(payload);

    GenesisRecord rec{
        .id = gen_id,
        .identity = req.identity,
        .constitution_digest = req.constitution_digest,
        .basal_state_digest = req.basal_state_digest,
        .logical_time = 0,
        .genesis_digest = gen_digest
    };

    record_ = rec;
    state_ = IdentityState{
        .id = req.identity,
        .genesis = gen_id,
        .lifecycle = LifecycleStatus::LifeActive,
        .current_time = 0
    };

    return rec;
}

bool GenesisService::verify(const GenesisRecord& rec) const noexcept {
    if (!record_.has_value()) {
        return false;
    }

    if (rec.id != record_->id || rec.identity != record_->identity) {
        return false;
    }

    std::string payload = std::format("{}:{}:{}:{}",
        rec.id.view(),
        rec.identity.view(),
        rec.constitution_digest.value,
        rec.basal_state_digest.value
    );
    core::Digest expected_digest = core::HashUtil::sha256(payload);

    return rec.genesis_digest == expected_digest && rec.genesis_digest == record_->genesis_digest;
}

} // namespace ente::identity
