#pragma once

#include "ente/identity/genesis_service.hpp"
#include "ente/identity/material_anchor.hpp"
#include "ente/authority/authority.hpp"
#include "ente/history/rec.hpp"
#include "ente/history/payloads.hpp"
#include "ente/epistemic/interpretation.hpp"
#include "ente/judgment/status_engine.hpp"
#include "ente/rcc/reassessment.hpp"
#include "ente/assurance/runtime_assurance.hpp"
#include "ente/constitution/verifier.hpp"
#include "ente/realization/domain.hpp"
#include <vector>
#include <memory>
#include <expected>
#include <unordered_map>

#include "ente/core/prng.hpp"

namespace ente::realization {

struct StepContext {
    std::string subject;
    std::string proposition;
    std::string step_desc{"step"};
};

struct DecisionTrace {
    judgment::CompatibilityResult compatibility{judgment::CompatibilityResult::Supported};
    rcc::EpistemicAction epistemic_action{rcc::EpistemicAction::Keep};
    rcc::RCCState rcc_state{rcc::RCCState::Stable};
    assurance::SafetyDirective safety_directive{assurance::SafetyDirective::AllowAction};
    constitution::ConstitutiveStatus constitutive_status{constitution::ConstitutiveStatus::Valid};
    std::optional<epistemic::Interpretation> current_interpretation;
    std::optional<epistemic::EvidenceRequest> evidence_request;
    core::Digest rec_head_hash;
    core::EventId rec_head_id;
    core::LogicalTime time{0};
    bool action_suspended{false};
};

struct ActionTransactionState {
    history::ActionTransactionPayload payload;
    core::EventId last_event_id;
    core::LogicalTime updated_at{0};
};

struct ScenarioStep {
    core::LogicalTime time;
    std::vector<epistemic::Observation> observations;
    std::string description;
};

struct ExecutionSummary {
    bool success{false};
    size_t total_events{0};
    bool rcc_triggered{false};
    bool action_suspended{false};
    bool coherence_restored{false};
    constitution::VerificationReport final_verification;
};

class EnteRealization {
public:
    explicit EnteRealization(
        std::unique_ptr<judgment::JudgmentEngine> engine =
            std::make_unique<judgment::StatusJudgmentEngine>());

    [[nodiscard]] std::expected<identity::GenesisRecord, core::EnteError> genesis(
        const core::IdentityId& id,
        std::optional<identity::MaterialAnchor> initial_anchor = std::nullopt,
        std::optional<core::Ed25519KeyPair> root_authority_signer = std::nullopt
    );

    // Hardware replacement under RIT (Ship of Theseus: S0 -> S1) preserving Identity
    [[nodiscard]] std::expected<identity::MaterialBinding, core::EnteError> migrate_hardware(
        identity::MaterialAnchor new_anchor,
        core::LogicalTime time
    );

    [[nodiscard]] std::expected<authority::AuthorityEpoch, core::EnteError> transition_authority(
        authority::AuthorityId new_authority,
        core::Ed25519KeyPair new_authority_signer,
        core::LogicalTime time
    );

    [[nodiscard]] std::expected<void, core::EnteError> attach_authority_signer(
        core::Ed25519KeyPair signer
    );

    // Cold Recovery from historical REC (restores identity, authority, material bindings, interpretation, and blocks 2nd Genesis)
    [[nodiscard]] static std::expected<EnteRealization, core::EnteError> recover_from_history(history::RecoverableHistory history);
    [[nodiscard]] static std::expected<EnteRealization, core::EnteError> recover_from_file(std::string_view filepath);
    [[nodiscard]] static std::expected<EnteRealization, core::EnteError> recover_from_authenticated_file(
        std::string_view filepath,
        core::Ed25519KeyPair signer
    );

    // Enables synchronous snapshot journaling. Once enabled, every factual
    // action phase is fsync'ed before the transition is returned to the caller.
    [[nodiscard]] std::expected<void, core::EnteError> enable_durable_journal(
        std::string_view filepath,
        history::PersistenceOptions options = {}
    );
    [[nodiscard]] std::expected<void, core::EnteError> enable_authenticated_durable_journal(
        std::string_view filepath,
        core::Ed25519KeyPair signer,
        history::PersistenceOptions options = {}
    );
    [[nodiscard]] bool has_durable_journal() const noexcept { return journal_path_.has_value(); }
    [[nodiscard]] bool has_authenticated_journal() const noexcept {
        return journal_signer_.has_value();
    }

    [[nodiscard]] std::expected<DecisionTrace, core::EnteError> step(
        core::LogicalTime time,
        const std::vector<epistemic::Observation>& observations,
        std::string_view step_desc = "step"
    );

    [[nodiscard]] std::expected<DecisionTrace, core::EnteError> step_with_context(
        core::LogicalTime time,
        const std::vector<epistemic::Observation>& observations,
        const StepContext& context
    );

    // ENTE-1 factual action lifecycle. Authorization is recorded by step_with_context;
    // these methods distinguish intent, dispatch, executor acknowledgement and observed effect.
    [[nodiscard]] std::expected<ActionTransactionState, core::EnteError> prepare_action(
        core::LogicalTime time,
        std::string_view proposed_action,
        std::string_view effective_action,
        assurance::SafetyDirective directive,
        std::string_view pre_state
    );

    [[nodiscard]] std::expected<ActionTransactionState, core::EnteError> dispatch_action(
        const core::ActionTransactionId& action_id,
        core::LogicalTime time
    );

    [[nodiscard]] std::expected<ActionTransactionState, core::EnteError> acknowledge_action(
        const core::ActionTransactionId& action_id,
        core::LogicalTime time,
        std::string_view executed_action,
        bool succeeded,
        std::string_view post_state,
        std::string_view executor_id = "domain-executor"
    );

    [[nodiscard]] std::expected<ActionTransactionState, core::EnteError> observe_action_effect(
        const core::ActionTransactionId& action_id,
        core::LogicalTime time,
        bool confirmed,
        std::string_view effect,
        std::string_view post_state,
        std::vector<epistemic::Observation> effect_observations = {}
    );

    [[nodiscard]] std::optional<ActionTransactionState> action_transaction(
        const core::ActionTransactionId& action_id
    ) const noexcept;

    [[nodiscard]] constitution::VerificationReport verify() const noexcept;

    [[nodiscard]] const identity::IdentityState& identity() const noexcept { return genesis_service_.state(); }
    [[nodiscard]] const identity::MaterialBindingRegistry& material_bindings() const noexcept { return bindings_; }
    [[nodiscard]] const history::RecoverableHistory& history() const noexcept { return rec_; }
    [[nodiscard]] history::RecoverableHistory& history_mut() noexcept {
        history_requires_full_audit_ = true;
        return rec_;
    } // privileged/testing access invalidates incremental trust
    [[nodiscard]] const std::optional<epistemic::Interpretation>& current_interpretation() const noexcept { return current_interpretation_; }
    [[nodiscard]] const SyntheticDomain& domain() const noexcept { return domain_; }
    [[nodiscard]] const authority::AuthorityLineage& authority_lineage() const noexcept { return authority_; }
    [[nodiscard]] const core::EventScopedPRNG& prng() const noexcept { return prng_; }

    [[nodiscard]] std::expected<void, core::EnteError> adopt_interpretation(
        epistemic::Interpretation new_interp
    );

private:
    [[nodiscard]] std::expected<ActionTransactionState, core::EnteError> append_action_phase(
        history::ActionTransactionPayload payload,
        history::EventKind kind,
        core::LogicalTime time,
        std::vector<core::EvidenceId> evidence_refs
    );

    [[nodiscard]] std::expected<void, core::EnteError> rebuild_action_transactions_from_history();

    identity::GenesisService genesis_service_;
    identity::MaterialBindingRegistry bindings_;
    authority::AuthorityLineage authority_;
    std::optional<core::Ed25519KeyPair> authority_signer_;
    history::RecoverableHistory rec_;
    std::unordered_map<std::string, ActionTransactionState> action_transactions_;
    std::optional<std::string> journal_path_;
    history::PersistenceOptions journal_options_;
    std::optional<core::Ed25519KeyPair> journal_signer_;
    core::EventScopedPRNG prng_;
    std::optional<epistemic::Interpretation> current_interpretation_;
    rcc::ContextReassessment rcc_;
    assurance::RuntimeAssurance assurance_;
    std::unique_ptr<judgment::JudgmentEngine> judgment_;
    constitution::ConstitutionVerifier verifier_;
    SyntheticDomain domain_;
    mutable constitution::ConstitutiveStatus constitutive_status_{constitution::ConstitutiveStatus::Valid};
    mutable bool history_requires_full_audit_{false};
};

class ScenarioRunner {
public:
    [[nodiscard]] static ExecutionSummary run_scenario(
        EnteRealization& ente,
        const std::vector<ScenarioStep>& scenario
    );
};

} // namespace ente::realization
