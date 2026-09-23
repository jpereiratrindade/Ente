#pragma once

#include "ente/core/types.hpp"
#include <vector>
#include <string>

namespace ente::history {

enum class EventKind : uint8_t {
    Genesis,
    Observation,
    Interpretation,
    Perturbation,
    Judgment,
    ActionIntended,
    ActionExecution,
    EpistemicAction,
    Reinterpretation,
    Adaptation,
    ConstitutiveWarning,
    ConstitutiveRepair,
    CoherenceRestored
};

[[nodiscard]] constexpr std::string_view to_string(EventKind k) noexcept {
    switch (k) {
        case EventKind::Genesis: return "GENESIS";
        case EventKind::Observation: return "OBSERVATION";
        case EventKind::Interpretation: return "INTERPRETATION";
        case EventKind::Perturbation: return "PERTURBATION";
        case EventKind::Judgment: return "JUDGMENT";
        case EventKind::ActionIntended: return "ACTION_INTENDED";
        case EventKind::ActionExecution: return "ACTION_EXECUTION";
        case EventKind::EpistemicAction: return "EPISTEMIC_ACTION";
        case EventKind::Reinterpretation: return "REINTERPRETATION";
        case EventKind::Adaptation: return "ADAPTATION";
        case EventKind::ConstitutiveWarning: return "CONSTITUTIVE_WARNING";
        case EventKind::ConstitutiveRepair: return "CONSTITUTIVE_REPAIR";
        case EventKind::CoherenceRestored: return "COHERENCE_RESTORED";
    }
    return "UNKNOWN_EVENT";
}

struct HistoryEvent {
    core::EventId id;
    EventKind kind;
    core::IdentityId identity;
    core::LogicalTime logical_time{0};

    core::Digest previous_event_digest; // Linear hash-chain linking (C10)
    std::vector<core::EventId> causal_predecessors; // RIT causal graph (RIT / C10)
    std::vector<core::EvidenceId> evidence_refs; // C4: Provenance
    std::string authority_id{"auth-root"}; // C14: Authority provenance
    std::string authority_epoch{"epoch-0"}; // C14: Authority Epoch

    std::string payload_content; // Explicit canonical serialization
    core::Digest payload_digest; // HASH(payload_content)
    core::Digest event_digest;   // HASH(id + kind + identity + time + prev_digest + payload_digest + causal_digests + auth)
};

} // namespace ente::history
