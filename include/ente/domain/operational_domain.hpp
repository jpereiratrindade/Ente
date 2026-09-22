#pragma once

#include "ente/assurance/runtime_assurance.hpp"
#include <string_view>
#include <string>

namespace ente::domain {

// Generic Abstract Operational Domain Interface
template <typename ActionType, typename StateType>
class OperationalDomain {
public:
    virtual ~OperationalDomain() = default;

    [[nodiscard]] virtual ActionType active_action() const noexcept = 0;
    [[nodiscard]] virtual StateType current_state() const noexcept = 0;
    [[nodiscard]] virtual bool is_suspended() const noexcept = 0;

    // Apply safety directive derived from constitutive RCC & RuntimeAssurance
    virtual void apply_safety_directive(assurance::SafetyDirective directive) noexcept = 0;
};

} // namespace ente::domain
