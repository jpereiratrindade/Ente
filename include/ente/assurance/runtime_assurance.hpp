#pragma once

#include "ente/rcc/actions.hpp"
#include "ente/constitution/verifier.hpp"
#include <string_view>

namespace ente::assurance {

enum class SafetyDirective : uint8_t {
    AllowAction,
    SafeHold,
    DegradePerformance,
    EmergencyStop
};

[[nodiscard]] constexpr std::string_view to_string(SafetyDirective d) noexcept {
    switch (d) {
        case SafetyDirective::AllowAction: return "ALLOW_ACTION";
        case SafetyDirective::SafeHold: return "SAFE_HOLD";
        case SafetyDirective::DegradePerformance: return "DEGRADE_PERFORMANCE";
        case SafetyDirective::EmergencyStop: return "EMERGENCY_STOP";
    }
    return "UNKNOWN_DIRECTIVE";
}

class RuntimeAssurance {
public:
    RuntimeAssurance() = default;

    // Evaluates operational safety based on epistemic action and constitutional validity
    [[nodiscard]] SafetyDirective evaluate_safety(
        rcc::EpistemicAction epistemic_action,
        constitution::ConstitutiveStatus constitutive_status
    ) const noexcept;
};

} // namespace ente::assurance
