#include "ente/attestation/rats.hpp"
#include "ente/core/hash.hpp"
#include <format>

namespace ente::attestation {

AttestationResult IndependentAttestationVerifier::evaluate(
    const AttestationEvidence& evidence,
    const core::Digest& expected_constitution_digest
) const noexcept {
    if (evidence.anchor.id.empty() || evidence.anchor.hardware_fingerprint.empty()) {
        return AttestationResult{
            .verdict = AppraisalVerdict::HardwareUnrecognized,
            .verifier_id = "independent-verifier-0",
            .evidence_digest = core::Digest(),
            .evaluated_at = evidence.measured_at,
            .satisfies_constitutional_floor = false
        };
    }

    if (evidence.configuration_digest != expected_constitution_digest) {
        return AttestationResult{
            .verdict = AppraisalVerdict::ConfigMismatch,
            .verifier_id = "independent-verifier-0",
            .evidence_digest = evidence.configuration_digest,
            .evaluated_at = evidence.measured_at,
            .satisfies_constitutional_floor = false
        };
    }

    std::string evidence_payload = std::format("{}:{}:{}",
        evidence.anchor.id.view(),
        evidence.configuration_digest.value,
        evidence.measured_at
    );
    core::Digest ev_digest = core::HashUtil::sha256(evidence_payload);

    return AttestationResult{
        .verdict = AppraisalVerdict::Trustworthy,
        .verifier_id = "independent-verifier-0",
        .evidence_digest = ev_digest,
        .evaluated_at = evidence.measured_at,
        .satisfies_constitutional_floor = true
    };
}

} // namespace ente::attestation
