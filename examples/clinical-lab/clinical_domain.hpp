#pragma once

#include <cstdint>
#include <string_view>
#include <string>
#include <vector>
#include "ente/assurance/runtime_assurance.hpp"

namespace clinicallab {

enum class InfusionAction : uint8_t {
    HoldTitration,
    TitrateUp,
    MaintainDose,
    EmergencyFlush
};

[[nodiscard]] constexpr std::string_view to_string(InfusionAction a) noexcept {
    switch (a) {
        case InfusionAction::HoldTitration: return "HOLD_TITRATION";
        case InfusionAction::TitrateUp: return "TITRATE_UP";
        case InfusionAction::MaintainDose: return "MAINTAIN_DOSE";
        case InfusionAction::EmergencyFlush: return "EMERGENCY_FLUSH";
    }
    return "UNKNOWN_ACTION";
}

enum class PumpState : uint8_t {
    Holding,
    Titrating,
    Maintaining,
    Stopped
};

[[nodiscard]] constexpr std::string_view to_string(PumpState s) noexcept {
    switch (s) {
        case PumpState::Holding: return "HOLDING";
        case PumpState::Titrating: return "TITRATING";
        case PumpState::Maintaining: return "MAINTAINING";
        case PumpState::Stopped: return "STOPPED";
    }
    return "UNKNOWN_STATE";
}

struct ClinicalObservation {
    std::string source;
    std::string subject;
    std::string value;
    std::string epistemic_tag; // "OBSERVED", "UNKNOWN", "CONTRADICTORY"
};

class InfusionPumpDomain {
public:
    using ActionType = InfusionAction;
    using StateType = PumpState;

    [[nodiscard]] InfusionAction active_action() const noexcept { return action_; }
    [[nodiscard]] PumpState current_state() const noexcept { return state_; }
    [[nodiscard]] bool is_suspended() const noexcept { return suspended_; }
    [[nodiscard]] static constexpr InfusionAction safe_hold_action() noexcept { return InfusionAction::HoldTitration; }

    void apply_safety_directive(ente::assurance::SafetyDirective directive) noexcept {
        if (directive == ente::assurance::SafetyDirective::SafeHold ||
            directive == ente::assurance::SafetyDirective::EmergencyStop ||
            directive == ente::assurance::SafetyDirective::DegradePerformance) {
            suspended_ = true;
            action_ = InfusionAction::HoldTitration;
            state_ = PumpState::Holding;
        } else {
            suspended_ = false;
            action_ = InfusionAction::TitrateUp;
            state_ = PumpState::Titrating;
        }
    }

private:
    InfusionAction action_{InfusionAction::HoldTitration};
    PumpState state_{PumpState::Stopped};
    bool suspended_{false};
};

} // namespace clinicallab
