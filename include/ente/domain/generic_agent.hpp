#pragma once

#include "ente/realization/runner.hpp"
#include "ente/domain/operational_domain.hpp"
#include <concepts>
#include <memory>
#include <vector>
#include <string>

namespace ente::domain {

// C++26 Concept for an Operational Domain that can be governed by an ENTE
template <typename T>
concept OperationalDomainConcept = requires(T domain, assurance::SafetyDirective directive) {
    typename T::ActionType;
    typename T::StateType;
    { domain.active_action() } -> std::same_as<typename T::ActionType>;
    { domain.current_state() } -> std::same_as<typename T::StateType>;
    { domain.is_suspended() } -> std::same_as<bool>;
    { domain.apply_safety_directive(directive) } noexcept;
    { domain.safe_hold_action() } -> std::same_as<typename T::ActionType>;
};

// Generic Universal Agent Mediated by ENTE-0
template <OperationalDomainConcept DomainT>
class GenericAgentWithEnte {
public:
    using ActionType = typename DomainT::ActionType;
    using StateType = typename DomainT::StateType;

    explicit GenericAgentWithEnte(std::string_view agent_id, DomainT domain = DomainT{})
        : id_(agent_id)
        , domain_(std::move(domain))
    {
        core::IdentityId ente_id(std::format("ente-{}", agent_id));
        auto gen_res = ente_.genesis(ente_id);
        if (!gen_res.has_value()) {
            throw std::runtime_error("Failed to establish ENTE Genesis for Generic Agent");
        }
    }

    [[nodiscard]] const realization::EnteRealization& ente() const noexcept { return ente_; }
    [[nodiscard]] realization::EnteRealization& ente_mut() noexcept { return ente_; }
    [[nodiscard]] const DomainT& domain() const noexcept { return domain_; }
    [[nodiscard]] DomainT& domain_mut() noexcept { return domain_; }

    // Execute decision cycle mediated by constitutive invariants
    ActionType decide_action(
        core::LogicalTime time,
        const std::vector<epistemic::Observation>& observations,
        ActionType proposed_action,
        std::string_view step_desc = "generic_cycle"
    ) {
        // 1. Submit step through ENTE constitutive pipeline (REC -> RCC -> Verifier -> Assurance)
        auto step_res = ente_.step(time, observations, step_desc);

        // 2. If ENTE suspended actions due to epistemic vulnerability or violation:
        if (!step_res.has_value() || ente_.domain().is_action_suspended()) {
            domain_.apply_safety_directive(assurance::SafetyDirective::SafeHold);
            return domain_.safe_hold_action();
        }

        // 3. Constitutionally certified nominal operation
        domain_.apply_safety_directive(assurance::SafetyDirective::AllowAction);
        return proposed_action;
    }

private:
    std::string id_;
    DomainT domain_;
    realization::EnteRealization ente_;
};

} // namespace ente::domain
