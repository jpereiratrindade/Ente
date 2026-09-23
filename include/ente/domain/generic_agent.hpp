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

// C++26 Concept for an Operational Domain that can be governed by an ENTE
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

template <typename ActionT>
struct DecisionOutcome {
    ActionT executed_action{};
    assurance::SafetyDirective directive{assurance::SafetyDirective::AllowAction};
    realization::DecisionTrace trace{};
    bool is_safe_hold{false};
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
        std::optional<identity::MaterialAnchor> initial_anchor = std::nullopt
    )
        : id_(agent_id)
        , domain_(std::move(domain))
    {
        core::IdentityId ente_id(std::format("ente-{}", agent_id));
        auto gen_res = ente_.genesis(ente_id, std::move(initial_anchor));
        if (!gen_res.has_value()) {
            throw std::runtime_error("Failed to establish ENTE Genesis for Generic Agent");
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

        // 2. If ENTE suspended actions due to epistemic vulnerability or violation:
        if (!step_res.has_value() || outcome.directive != assurance::SafetyDirective::AllowAction || ente_.domain().is_action_suspended()) {
            domain_.apply_safety_directive(assurance::SafetyDirective::SafeHold);
            outcome.executed_action = domain_.safe_hold_action();
            outcome.is_safe_hold = true;
            return outcome;
        }

        // 3. Constitutionally certified nominal operation: domain applies the PROPOSED action
        domain_.apply_action(proposed_action);
        outcome.executed_action = proposed_action;
        outcome.is_safe_hold = false;
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
