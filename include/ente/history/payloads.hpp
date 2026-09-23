#pragma once

#include "ente/core/types.hpp"
#include "ente/epistemic/status.hpp"
#include "ente/assurance/runtime_assurance.hpp"
#include "ente/rcc/actions.hpp"
#include <string>
#include <vector>
#include <optional>
#include <format>

namespace ente::history {

struct GenesisPayload {
    core::IdentityId identity;
    core::Digest genesis_digest;
    std::string material_anchor_id;
    std::string hardware_fingerprint;
    std::string substrate_type;
};

struct ObservationPayload {
    core::EvidenceId evidence_id;
    std::string source;
    std::string subject;
    std::string value;
    epistemic::EpistemicStatus status{epistemic::EpistemicStatus::Observed};
    core::LogicalTime observed_at{0};
};

struct InterpretationPayload {
    core::InterpretationId id;
    std::string subject;
    std::string proposition;
    std::vector<core::EvidenceId> supporting_evidence;
    std::vector<core::EvidenceId> challenging_evidence;
    std::optional<core::InterpretationId> supersedes;
    core::LogicalTime created_at{0};
};

struct ActionIntentPayload {
    std::string action_id;
    std::string proposed_action;
    assurance::SafetyDirective safety_directive{assurance::SafetyDirective::AllowAction};
    rcc::EpistemicAction epistemic_action{rcc::EpistemicAction::Keep};
    std::string pre_state;
    std::string reason;
};

struct ActionExecutionAckPayload {
    std::string action_id;
    std::string executed_action;
    std::string execution_status; // e.g. "ACK_SUCCESS", "SAFE_HOLD", "EXECUTION_FAILED"
    std::string executor_id;
};

struct EffectObservationPayload {
    std::string action_id;
    std::string confirmed_effect; // e.g. "FLOW_OBSERVED_18.5_LMIN", "LIMIT_SWITCH_OPEN"
    std::string post_state;
    std::vector<core::EvidenceId> telemetry_evidence_refs;
    bool effect_confirmed{false};
};

struct MaterialMigrationPayload {
    std::string new_anchor_id;
    std::string new_hardware_fingerprint;
    std::string substrate_type;
    std::optional<std::string> previous_anchor_id;
};

struct AuthorityTransitionPayload {
    std::string new_epoch_id;
    std::string previous_epoch_id;
    std::string new_authority_id;
    core::LogicalTime transition_time{0};
    core::Digest predecessor_epoch_digest;
    std::string signature;
    std::string delegation_policy{"STRICT_LINEAGE"};
};

struct PerturbationPayload {
    std::string compatibility;
    std::string reason;
    rcc::EpistemicAction recommended_action{rcc::EpistemicAction::Keep};
};

} // namespace ente::history
