#pragma once

#include "ente/core/types.hpp"
#include "ente/identity/material_anchor.hpp"
#include <string>
#include <vector>

namespace ente::attestation {

struct SoftwareMeasurement {
    std::string component_name;
    core::Digest code_digest;
    std::string version;
};

// Evidence emitted by Attester (Substrate / Enclave / Hardware Anchor)
struct AttestationEvidence {
    identity::MaterialAnchor anchor;
    std::vector<SoftwareMeasurement> measurements;
    core::Digest configuration_digest;
    core::LogicalTime measured_at{0};
    std::string nonce{"nonce-0"}; // Freshness challenge against replay attacks
    core::Digest attestation_signature;
};

enum class AppraisalVerdict : uint8_t {
    TrustworthyVerified,
    StructurallyAcceptable,
    ConfigMismatch,
    HardwareUnrecognized,
    MeasurementMismatch,
    Untrusted
};

[[nodiscard]] constexpr std::string_view to_string(AppraisalVerdict v) noexcept {
    switch (v) {
        case AppraisalVerdict::TrustworthyVerified: return "TRUSTWORTHY_VERIFIED";
        case AppraisalVerdict::StructurallyAcceptable: return "STRUCTURALLY_ACCEPTABLE";
        case AppraisalVerdict::ConfigMismatch: return "CONFIG_MISMATCH";
        case AppraisalVerdict::HardwareUnrecognized: return "HARDWARE_UNRECOGNIZED";
        case AppraisalVerdict::MeasurementMismatch: return "MEASUREMENT_MISMATCH";
        case AppraisalVerdict::Untrusted: return "UNTRUSTED";
    }
    return "UNKNOWN_VERDICT";
}

// Result emitted by independent Verifier (RATS Architecture)
struct AttestationResult {
    AppraisalVerdict verdict;
    std::string verifier_id;
    core::Digest evidence_digest;
    core::LogicalTime evaluated_at{0};
    bool satisfies_constitutional_floor{false};
};

class IndependentAttestationVerifier {
public:
    IndependentAttestationVerifier() = default;

    [[nodiscard]] AttestationResult evaluate(
        const AttestationEvidence& evidence,
        const core::Digest& expected_constitution_digest
    ) const noexcept;
};

} // namespace ente::attestation
