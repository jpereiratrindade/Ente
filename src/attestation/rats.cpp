#include "ente/attestation/rats.hpp"
#include "ente/core/hash.hpp"
#include <format>

namespace ente::attestation {

AttestationResult IndependentAttestationVerifier::evaluate(
    const AttestationEvidence& evidence,
    const core::Digest& expected_constitution_digest
) const noexcept {
    // 1. Hardware Anchor Fingerprint Validation
    if (evidence.anchor.id.empty() || evidence.anchor.hardware_fingerprint.empty()) {
        return AttestationResult{
            .verdict = AppraisalVerdict::HardwareUnrecognized,
            .verifier_id = "independent-verifier-0",
            .evidence_digest = core::Digest(),
            .evaluated_at = evidence.measured_at,
            .satisfies_constitutional_floor = false
        };
    }

    // 2. Configuration & Constitution Digest Validation
    if (evidence.configuration_digest != expected_constitution_digest) {
        return AttestationResult{
            .verdict = AppraisalVerdict::ConfigMismatch,
            .verifier_id = "independent-verifier-0",
            .evidence_digest = evidence.configuration_digest,
            .evaluated_at = evidence.measured_at,
            .satisfies_constitutional_floor = false
        };
    }

    // 3. Software Measurements Validation (Ensure no empty components)
    for (const auto& m : evidence.measurements) {
        if (m.component_name.empty() || m.code_digest.is_zero()) {
            return AttestationResult{
                .verdict = AppraisalVerdict::MeasurementMismatch,
                .verifier_id = "independent-verifier-0",
                .evidence_digest = evidence.configuration_digest,
                .evaluated_at = evidence.measured_at,
                .satisfies_constitutional_floor = false
            };
        }
    }

    // 4. Nonce / Freshness Validation (Prevent replay attacks)
    if (evidence.nonce.empty()) {
        return AttestationResult{
            .verdict = AppraisalVerdict::Untrusted,
            .verifier_id = "independent-verifier-0",
            .evidence_digest = evidence.configuration_digest,
            .evaluated_at = evidence.measured_at,
            .satisfies_constitutional_floor = false
        };
    }

    // 5. Cryptographic Attestation Signature Verification
    std::string evidence_payload = std::format("{}:{}:{}:{}",
        evidence.anchor.id.view(),
        evidence.configuration_digest.value,
        evidence.nonce,
        evidence.measured_at
    );
    core::Digest expected_signature = core::HashUtil::combine(
        core::HashUtil::sha256(evidence.anchor.hardware_fingerprint),
        evidence_payload
    );

    if (evidence.attestation_signature.is_zero()) {
        // Evidence lacks cryptographic attestation signature -> fails constitutional floor
        return AttestationResult{
            .verdict = AppraisalVerdict::StructurallyAcceptable,
            .verifier_id = "independent-verifier-0",
            .evidence_digest = core::HashUtil::sha256(evidence_payload),
            .evaluated_at = evidence.measured_at,
            .satisfies_constitutional_floor = false
        };
    }

    if (evidence.attestation_signature != expected_signature) {
        return AttestationResult{
            .verdict = AppraisalVerdict::Untrusted,
            .verifier_id = "independent-verifier-0",
            .evidence_digest = core::HashUtil::sha256(evidence_payload),
            .evaluated_at = evidence.measured_at,
            .satisfies_constitutional_floor = false
        };
    }

    return AttestationResult{
        .verdict = AppraisalVerdict::TrustworthyVerified,
        .verifier_id = "independent-verifier-0",
        .evidence_digest = expected_signature,
        .evaluated_at = evidence.measured_at,
        .satisfies_constitutional_floor = true
    };
}

} // namespace ente::attestation
