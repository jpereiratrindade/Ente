#pragma once

#include "ente/core/hash.hpp"
#include "ente/history/payloads.hpp"

#include <string>
#include <string_view>

namespace ente::testing {

inline std::string canonical_genesis_payload(const core::IdentityId& identity) {
    return history::serialize_genesis_payload({
        .identity = identity,
        .genesis_digest = core::HashUtil::sha256(
            std::string("TEST_GENESIS:") + std::string(identity.view())),
        .material_anchor_id = "test-anchor",
        .hardware_fingerprint = "test-fingerprint",
        .substrate_type = "SIMULATED_MEMORY",
        .root_authority_public_key = std::string(64, '0')
    });
}

inline std::string canonical_observation_payload(
    const core::EvidenceId& evidence,
    core::LogicalTime observed_at,
    std::string_view value = "observed"
) {
    return history::serialize_observation_payload({
        .evidence_id = evidence,
        .source = "test-source",
        .subject = "test-subject",
        .value = std::string(value),
        .status = epistemic::EpistemicStatus::Observed,
        .observed_at = observed_at
    });
}

} // namespace ente::testing
