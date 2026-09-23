#pragma once

#include "ente/core/types.hpp"
#include "ente/epistemic/status.hpp"
#include "ente/assurance/runtime_assurance.hpp"
#include "ente/rcc/actions.hpp"
#include <string>
#include <vector>
#include <optional>
#include <format>
#include <expected>

namespace ente::history {

enum class ActionPhase : uint8_t {
    Prepared,
    Authorized,
    Dispatched,
    Acknowledged,
    EffectUnconfirmed,
    Confirmed,
    Failed,
    RecoveryRequired
};

[[nodiscard]] constexpr std::string_view to_string(ActionPhase phase) noexcept {
    switch (phase) {
        case ActionPhase::Prepared: return "PREPARED";
        case ActionPhase::Authorized: return "AUTHORIZED";
        case ActionPhase::Dispatched: return "DISPATCHED";
        case ActionPhase::Acknowledged: return "ACKNOWLEDGED";
        case ActionPhase::EffectUnconfirmed: return "EFFECT_UNCONFIRMED";
        case ActionPhase::Confirmed: return "CONFIRMED";
        case ActionPhase::Failed: return "FAILED";
        case ActionPhase::RecoveryRequired: return "RECOVERY_REQUIRED";
    }
    return "INVALID_ACTION_PHASE";
}

struct ActionTransactionPayload {
    core::ActionTransactionId action_id;
    ActionPhase phase{ActionPhase::Prepared};
    std::string proposed_action;
    std::string effective_action;
    assurance::SafetyDirective safety_directive{assurance::SafetyDirective::SafeHold};
    std::string pre_state;
    std::string post_state;
    std::string detail;
};

// Length-prefixed canonical representation. Values may contain arbitrary delimiters.
[[nodiscard]] std::string serialize_action_transaction(const ActionTransactionPayload& payload);
[[nodiscard]] std::expected<ActionTransactionPayload, core::EnteError> parse_action_transaction(
    std::string_view serialized
) noexcept;

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

[[nodiscard]] std::string serialize_genesis_payload(const GenesisPayload& payload);
[[nodiscard]] std::expected<GenesisPayload, core::EnteError> parse_genesis_payload(
    std::string_view serialized
) noexcept;

[[nodiscard]] std::string serialize_observation_payload(const ObservationPayload& payload);
[[nodiscard]] std::expected<ObservationPayload, core::EnteError> parse_observation_payload(
    std::string_view serialized
) noexcept;

[[nodiscard]] std::string serialize_interpretation_payload(const InterpretationPayload& payload);
[[nodiscard]] std::expected<InterpretationPayload, core::EnteError> parse_interpretation_payload(
    std::string_view serialized
) noexcept;

[[nodiscard]] std::string serialize_material_migration_payload(const MaterialMigrationPayload& payload);
[[nodiscard]] std::expected<MaterialMigrationPayload, core::EnteError> parse_material_migration_payload(
    std::string_view serialized
) noexcept;

struct PerturbationPayload {
    std::string compatibility;
    std::string reason;
    rcc::EpistemicAction recommended_action{rcc::EpistemicAction::Keep};
};

} // namespace ente::history
