#pragma once

#include "ente/core/types.hpp"
#include "ente/identity/identity.hpp"
#include <expected>
#include <optional>

namespace ente::identity {

struct GenesisRequest {
    core::IdentityId identity;
    core::Digest constitution_digest;
    core::Digest basal_state_digest;
};

class GenesisService {
public:
    GenesisService() = default;

    // Enforce C1/C9: Genesis occurs exactly once per identity
    [[nodiscard]] std::expected<GenesisRecord, core::EnteError> create_genesis(const GenesisRequest& req) noexcept;

    [[nodiscard]] bool has_genesis() const noexcept { return record_.has_value(); }
    [[nodiscard]] const std::optional<GenesisRecord>& record() const noexcept { return record_; }
    [[nodiscard]] const IdentityState& state() const noexcept { return state_; }

    [[nodiscard]] bool verify(const GenesisRecord& rec) const noexcept;

private:
    std::optional<GenesisRecord> record_;
    IdentityState state_;
};

} // namespace ente::identity
