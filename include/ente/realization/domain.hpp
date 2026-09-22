#pragma once

#include "ente/core/types.hpp"
#include "ente/epistemic/observation.hpp"
#include <vector>
#include <string>

namespace ente::realization {

// Synthetic domain for testing RCC without real hardware/sensors
class SyntheticDomain {
public:
    SyntheticDomain() = default;

    enum class Action : uint8_t {
        None,
        MoveForward,
        HoldPosition,
        SeekEvidence
    };

    [[nodiscard]] Action active_action() const noexcept { return active_action_; }
    void set_active_action(Action a) noexcept { active_action_ = a; }

    [[nodiscard]] bool is_action_suspended() const noexcept { return action_suspended_; }
    void suspend_action() noexcept {
        action_suspended_ = true;
        active_action_ = Action::HoldPosition;
    }
    void resume_action(Action a) noexcept {
        action_suspended_ = false;
        active_action_ = a;
    }

private:
    Action active_action_{Action::None};
    bool action_suspended_{false};
};

[[nodiscard]] constexpr std::string_view to_string(SyntheticDomain::Action a) noexcept {
    switch (a) {
        case SyntheticDomain::Action::None: return "NONE";
        case SyntheticDomain::Action::MoveForward: return "MOVE_FORWARD";
        case SyntheticDomain::Action::HoldPosition: return "HOLD_POSITION";
        case SyntheticDomain::Action::SeekEvidence: return "SEEK_EVIDENCE";
    }
    return "UNKNOWN_ACTION";
}

} // namespace ente::realization
