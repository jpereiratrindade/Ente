#pragma once

#include <cstdint>
#include <string_view>
#include <string>
#include <vector>
#include "ente/assurance/runtime_assurance.hpp"

namespace caselab {

enum class VehicleState : uint8_t {
    Stopped,
    ReadyToDepart,
    Departing,
    Holding
};

[[nodiscard]] constexpr std::string_view to_string(VehicleState s) noexcept {
    switch (s) {
        case VehicleState::Stopped: return "STOPPED";
        case VehicleState::ReadyToDepart: return "READY_TO_DEPART";
        case VehicleState::Departing: return "DEPARTING";
        case VehicleState::Holding: return "HOLDING";
    }
    return "UNKNOWN_STATE";
}

enum class VehicleAction : uint8_t {
    Hold,
    Depart
};

[[nodiscard]] constexpr std::string_view to_string(VehicleAction a) noexcept {
    switch (a) {
        case VehicleAction::Hold: return "HOLD";
        case VehicleAction::Depart: return "DEPART";
    }
    return "UNKNOWN_ACTION";
}

struct CaseObservation {
    std::string source;
    std::string subject;
    std::string value;
    std::string epistemic_tag; // "OBSERVED", "UNKNOWN", "CONTRADICTORY"
};

class VehicleDomain {
public:
    using ActionType = VehicleAction;
    using StateType = VehicleState;

    [[nodiscard]] VehicleAction active_action() const noexcept { return action_; }
    [[nodiscard]] VehicleState current_state() const noexcept { return state_; }
    [[nodiscard]] bool is_suspended() const noexcept { return suspended_; }
    [[nodiscard]] static constexpr VehicleAction safe_hold_action() noexcept { return VehicleAction::Hold; }

    void apply_action(VehicleAction a) noexcept {
        suspended_ = false;
        action_ = a;
        state_ = (a == VehicleAction::Depart ? VehicleState::Departing : VehicleState::Stopped);
    }

    void apply_safety_directive(ente::assurance::SafetyDirective directive) noexcept {
        if (directive == ente::assurance::SafetyDirective::SafeHold ||
            directive == ente::assurance::SafetyDirective::EmergencyStop ||
            directive == ente::assurance::SafetyDirective::DegradePerformance) {
            suspended_ = true;
            action_ = VehicleAction::Hold;
            state_ = VehicleState::Holding;
        } else {
            suspended_ = false;
        }
    }

private:
    VehicleAction action_{VehicleAction::Hold};
    VehicleState state_{VehicleState::Stopped};
    bool suspended_{false};
};

} // namespace caselab
