#pragma once

#include "ente/realization/runner.hpp"
#include "ente/domain/operational_domain.hpp"
#include <concepts>
#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <format>

namespace ente::domain {

// C++23 concept for an Operational Domain that can be governed by an ENTE
template <typename T>
concept OperationalDomainConcept = requires(T domain, typename T::ActionType action, assurance::SafetyDirective directive) {
    typename T::ActionType;
    typename T::StateType;
    { domain.active_action() } -> std::same_as<typename T::ActionType>;
    { domain.current_state() } -> std::same_as<typename T::StateType>;
    { domain.is_suspended() } -> std::same_as<bool>;
    { domain.apply_safety_directive(directive) } noexcept;
    { domain.apply_action(action) } noexcept;
    { domain.safe_hold_action() } -> std::same_as<typename T::ActionType>;
};

template <typename T>
std::string to_domain_string(const T& val) {
    if constexpr (requires { to_string(val); }) {
        return std::string(to_string(val));
    } else if constexpr (requires { std::to_string(val); }) {
        return std::to_string(val);
    } else if constexpr (requires { std::format("{}", val); }) {
        return std::format("{}", val);
    } else {
        return "STATE_VALUE";
    }
}

template <typename ActionT>
struct DecisionOutcome {
    ActionT executed_action{};
    assurance::SafetyDirective directive{assurance::SafetyDirective::AllowAction};
    realization::DecisionTrace trace{};
    bool is_safe_hold{false};
    std::optional<realization::ActionTransactionState> action_transaction;
    std::optional<core::EnteError> audit_error;
    bool effect_confirmed{false};
};

// Generic Universal Agent Mediated by ENTE-0
template <OperationalDomainConcept DomainT>
class GenericAgentWithEnte {
public:
    using ActionType = typename DomainT::ActionType;
    using StateType = typename DomainT::StateType;

    explicit GenericAgentWithEnte(
        std::string_view agent_id,
        DomainT domain = DomainT{},
        std::optional<identity::MaterialAnchor> initial_anchor = std::nullopt,
        std::optional<std::string> journal_path = std::nullopt,
        history::PersistenceOptions journal_options = {},
        std::optional<core::Ed25519KeyPair> journal_signer = std::nullopt,
        std::unique_ptr<judgment::JudgmentEngine> judgment_engine =
            std::make_unique<judgment::StatusJudgmentEngine>()
    )
        : id_(agent_id)
        , domain_(std::move(domain))
        , ente_(std::move(judgment_engine))
    {
        core::IdentityId ente_id(std::format("ente-{}", agent_id));
        auto gen_res = ente_.genesis(ente_id, std::move(initial_anchor));
        if (!gen_res.has_value()) {
            throw std::runtime_error("Failed to establish ENTE Genesis for Generic Agent");
        }
        if (journal_path.has_value()) {
            auto journal_result = journal_signer.has_value()
                ? ente_.enable_authenticated_durable_journal(
                    *journal_path,
                    std::move(*journal_signer),
                    journal_options
                )
                : ente_.enable_durable_journal(*journal_path, journal_options);
            if (!journal_result.has_value()) {
                throw std::runtime_error("Failed to establish ENTE durable journal");
            }
        }
    }

    [[nodiscard]] const realization::EnteRealization& ente() const noexcept { return ente_; }
    [[nodiscard]] realization::EnteRealization& ente_mut() noexcept { return ente_; }
    [[nodiscard]] const DomainT& domain() const noexcept { return domain_; }
    [[nodiscard]] DomainT& domain_mut() noexcept { return domain_; }

    // Execute decision cycle with full factual DecisionOutcome trace
    DecisionOutcome<ActionType> decide_action_detailed(
        core::LogicalTime time,
        const std::vector<epistemic::Observation>& observations,
        ActionType proposed_action,
        const realization::StepContext& context = realization::StepContext{}
    ) {
        std::string pre_state_str = to_domain_string(domain_.current_state());

        // 1. Submit step through ENTE constitutive pipeline (REC -> RCC -> Verifier -> Assurance)
        auto step_res = ente_.step_with_context(time, observations, context);

        DecisionOutcome<ActionType> outcome;
        if (step_res.has_value()) {
            outcome.trace = *step_res;
            outcome.directive = step_res->safety_directive;
        } else {
            outcome.directive = assurance::SafetyDirective::SafeHold;
            outcome.trace.safety_directive = assurance::SafetyDirective::SafeHold;
            outcome.trace.action_suspended = true;
        }

        const bool must_hold = !step_res.has_value() ||
            outcome.directive != assurance::SafetyDirective::AllowAction ||
            ente_.domain().is_action_suspended();
        const ActionType selected_action = must_hold ? domain_.safe_hold_action() : proposed_action;

        // A failed constitutional step cannot authorize a transaction. The
        // operational substrate still receives the fail-safe directive.
        if (!step_res.has_value()) {
            domain_.apply_safety_directive(assurance::SafetyDirective::SafeHold);
            outcome.executed_action = domain_.safe_hold_action();
            outcome.is_safe_hold = true;
            outcome.audit_error = step_res.error();
            return outcome;
        }

        // 2. Persist the exact proposed/effective action intent before dispatch.
        auto prepared = ente_.prepare_action(
            time,
            to_domain_string(proposed_action),
            to_domain_string(selected_action),
            outcome.directive,
            pre_state_str
        );
        if (!prepared.has_value()) {
            domain_.apply_safety_directive(assurance::SafetyDirective::SafeHold);
            outcome.executed_action = domain_.safe_hold_action();
            outcome.is_safe_hold = true;
            outcome.audit_error = prepared.error();
            return outcome;
        }
        outcome.action_transaction = *prepared;

        // 3. Record dispatch before crossing the physical side-effect boundary.
        auto dispatched = ente_.dispatch_action(prepared->payload.action_id, time);
        if (!dispatched.has_value()) {
            domain_.apply_safety_directive(assurance::SafetyDirective::SafeHold);
            outcome.executed_action = domain_.safe_hold_action();
            outcome.is_safe_hold = true;
            outcome.audit_error = dispatched.error();
            return outcome;
        }
        outcome.action_transaction = *dispatched;

        // 4. Execute the effective action selected by constitutional assurance.
        if (must_hold) {
            domain_.apply_safety_directive(assurance::SafetyDirective::SafeHold);
            outcome.is_safe_hold = true;
        } else {
            domain_.apply_action(proposed_action);
            outcome.is_safe_hold = false;
        }
        outcome.executed_action = selected_action;

        // 5. Record executor acknowledgement. This does not claim physical effect.
        std::string post_state_str = to_domain_string(domain_.current_state());
        auto acknowledged = ente_.acknowledge_action(
            prepared->payload.action_id,
            time,
            to_domain_string(outcome.executed_action),
            true,
            post_state_str
        );
        if (!acknowledged.has_value()) {
            domain_.apply_safety_directive(assurance::SafetyDirective::SafeHold);
            outcome.is_safe_hold = true;
            outcome.audit_error = acknowledged.error();
            return outcome;
        }
        outcome.action_transaction = *acknowledged;

        // 6. The executor-reported state is not independent effect evidence.
        // Record that distinction explicitly and leave confirmation to telemetry.
        auto unconfirmed = ente_.observe_action_effect(
            prepared->payload.action_id,
            time,
            false,
            "EXECUTOR_STATE_REPORTED_WITHOUT_INDEPENDENT_EFFECT_EVIDENCE",
            post_state_str
        );
        if (!unconfirmed.has_value()) {
            domain_.apply_safety_directive(assurance::SafetyDirective::SafeHold);
            outcome.is_safe_hold = true;
            outcome.audit_error = unconfirmed.error();
            return outcome;
        }
        outcome.action_transaction = *unconfirmed;
        return outcome;
    }

    // Direct simple decision
    ActionType decide_action(
        core::LogicalTime time,
        const std::vector<epistemic::Observation>& observations,
        ActionType proposed_action,
        std::string_view step_desc = "generic_cycle"
    ) {
        realization::StepContext ctx{
            .subject = "domain_governance",
            .proposition = "Operação nominal de domínio",
            .step_desc = std::string(step_desc)
        };
        return decide_action_detailed(time, observations, proposed_action, ctx).executed_action;
    }

private:
    std::string id_;
    DomainT domain_;
    realization::EnteRealization ente_;
};

} // namespace ente::domain
