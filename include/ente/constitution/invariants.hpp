#pragma once

#include <cstdint>
#include <string_view>

namespace ente::constitution {

// Constitutive Invariants (Constitution v0.6.0 §27)
enum class InvariantId : uint8_t {
    C1_Identity,
    C2_Continuity,
    C3_Observability,
    C4_Provenance,
    C5_EpistemicDistinction,
    C6_RevisionCapability,
    C7_CoherenceEvaluation,
    C8_UnknownRepresentability,
    C9_GenesisAnchor,
    C10_TemporalIntegrity,
    C11_LineageSingularity,
    C12_HistoryRecoverability,
    C13_ConstitutiveFinalitySafety,
    C14_ConstitutiveAuthorityContinuity
};

[[nodiscard]] constexpr std::string_view to_string(InvariantId id) noexcept {
    switch (id) {
        case InvariantId::C1_Identity: return "C1_IDENTITY";
        case InvariantId::C2_Continuity: return "C2_CONTINUITY";
        case InvariantId::C3_Observability: return "C3_OBSERVABILITY";
        case InvariantId::C4_Provenance: return "C4_PROVENANCE";
        case InvariantId::C5_EpistemicDistinction: return "C5_EPISTEMIC_DISTINCTION";
        case InvariantId::C6_RevisionCapability: return "C6_REVISION_CAPABILITY";
        case InvariantId::C7_CoherenceEvaluation: return "C7_COHERENCE_EVALUATION";
        case InvariantId::C8_UnknownRepresentability: return "C8_UNKNOWN_REPRESENTABILITY";
        case InvariantId::C9_GenesisAnchor: return "C9_GENESIS_ANCHOR";
        case InvariantId::C10_TemporalIntegrity: return "C10_TEMPORAL_INTEGRITY";
        case InvariantId::C11_LineageSingularity: return "C11_LINEAGE_SINGULARITY";
        case InvariantId::C12_HistoryRecoverability: return "C12_HISTORY_RECOVERABILITY";
        case InvariantId::C13_ConstitutiveFinalitySafety: return "C13_CONSTITUTIVE_FINALITY_SAFETY";
        case InvariantId::C14_ConstitutiveAuthorityContinuity: return "C14_CONSTITUTIVE_AUTHORITY_CONTINUITY";
    }
    return "UNKNOWN_INVARIANT";
}

enum class InvariantStatus : uint8_t {
    Satisfied,
    Violated,
    Unknown,
    NotApplicable
};

[[nodiscard]] constexpr std::string_view to_string(InvariantStatus s) noexcept {
    switch (s) {
        case InvariantStatus::Satisfied: return "SATISFIED";
        case InvariantStatus::Violated: return "VIOLATED";
        case InvariantStatus::Unknown: return "UNKNOWN";
        case InvariantStatus::NotApplicable: return "NOT_APPLICABLE";
    }
    return "INVALID_STATUS";
}

} // namespace ente::constitution
