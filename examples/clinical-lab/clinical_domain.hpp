#pragma once

#include <cstdint>
#include <string_view>
#include <string>
#include <vector>

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

} // namespace clinicallab
