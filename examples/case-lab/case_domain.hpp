#pragma once

#include <cstdint>
#include <string_view>
#include <string>
#include <vector>

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

} // namespace caselab
