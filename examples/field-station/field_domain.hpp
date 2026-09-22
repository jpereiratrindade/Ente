#pragma once

#include <cstdint>
#include <string_view>
#include <string>
#include <vector>
#include "ente/assurance/runtime_assurance.hpp"
#include "ente/domain/generic_agent.hpp"

namespace fieldstation {

// ---------------------------------------------------------
// Domain States & Actions
// ---------------------------------------------------------

enum class IrrigationState : uint8_t {
    Idle,
    Evaluating,
    Irrigating,
    SafeHold
};

[[nodiscard]] constexpr std::string_view to_string(IrrigationState s) noexcept {
    switch (s) {
        case IrrigationState::Idle: return "IDLE";
        case IrrigationState::Evaluating: return "EVALUATING";
        case IrrigationState::Irrigating: return "IRRIGATING";
        case IrrigationState::SafeHold: return "SAFE_HOLD";
    }
    return "UNKNOWN_STATE";
}

enum class ValveAction : uint8_t {
    CloseValve,
    OpenValve
};

[[nodiscard]] constexpr std::string_view to_string(ValveAction a) noexcept {
    switch (a) {
        case ValveAction::CloseValve: return "CLOSE_VALVE";
        case ValveAction::OpenValve: return "OPEN_VALVE";
    }
    return "UNKNOWN_ACTION";
}

// ---------------------------------------------------------
// Field Sensor Telemetry & Diagnostics
// ---------------------------------------------------------

enum class SensorHealth : uint8_t {
    Healthy,
    CalibrationDrift,
    Disconnected
};

[[nodiscard]] constexpr std::string_view to_string(SensorHealth h) noexcept {
    switch (h) {
        case SensorHealth::Healthy: return "HEALTHY";
        case SensorHealth::CalibrationDrift: return "CALIBRATION_DRIFT";
        case SensorHealth::Disconnected: return "DISCONNECTED";
    }
    return "UNKNOWN_HEALTH";
}

enum class MaintenanceRecommendation : uint8_t {
    None,
    CalibrateProbeA,
    CalibrateProbeB,
    ReplaceProbe
};

struct SensorTelemetry {
    double soil_moisture_a{21.0}; // percentage
    double soil_moisture_b{22.0}; // redundant probe
    std::optional<double> soil_moisture_c{std::nullopt}; // reference probe C (active during diagnostics)
    
    SensorHealth health_sensor_a{SensorHealth::Healthy};
    SensorHealth health_sensor_b{SensorHealth::Healthy};
    
    bool rain_detected{false};
    double water_tank_level{73.0}; // percentage
    bool flow_sensor_ok{true};
    std::string hardware_id{"RP-A921"};
    
    bool diagnostic_active{false};
    MaintenanceRecommendation recommendation{MaintenanceRecommendation::None};
};

// ---------------------------------------------------------
// Field Station Operational Domain (satisfies OperationalDomainConcept)
// ---------------------------------------------------------

class FieldStationDomain {
public:
    using ActionType = ValveAction;
    using StateType = IrrigationState;

    [[nodiscard]] ValveAction active_action() const noexcept { return action_; }
    [[nodiscard]] IrrigationState current_state() const noexcept { return state_; }
    [[nodiscard]] bool is_suspended() const noexcept { return suspended_; }
    [[nodiscard]] static constexpr ValveAction safe_hold_action() noexcept { return ValveAction::CloseValve; }
    [[nodiscard]] const SensorTelemetry& telemetry() const noexcept { return telemetry_; }

    void set_telemetry(SensorTelemetry t) noexcept {
        telemetry_ = std::move(t);
    }

    void run_diagnostic_self_test(bool probe_b_has_drift, double reference_c_reading) noexcept {
        telemetry_.diagnostic_active = true;
        telemetry_.soil_moisture_c = reference_c_reading;
        if (probe_b_has_drift) {
            telemetry_.health_sensor_a = SensorHealth::Healthy;
            telemetry_.health_sensor_b = SensorHealth::CalibrationDrift;
            telemetry_.recommendation = MaintenanceRecommendation::CalibrateProbeB;
        } else {
            telemetry_.health_sensor_a = SensorHealth::CalibrationDrift;
            telemetry_.health_sensor_b = SensorHealth::Healthy;
            telemetry_.recommendation = MaintenanceRecommendation::CalibrateProbeA;
        }
    }

    void apply_safety_directive(ente::assurance::SafetyDirective directive) noexcept {
        if (directive == ente::assurance::SafetyDirective::SafeHold ||
            directive == ente::assurance::SafetyDirective::EmergencyStop ||
            directive == ente::assurance::SafetyDirective::DegradePerformance) {
            suspended_ = true;
            action_ = ValveAction::CloseValve;
            state_ = IrrigationState::SafeHold;
        } else {
            suspended_ = false;
            action_ = ValveAction::OpenValve;
            state_ = IrrigationState::Irrigating;
        }
    }

private:
    ValveAction action_{ValveAction::CloseValve};
    IrrigationState state_{IrrigationState::Idle};
    bool suspended_{false};
    SensorTelemetry telemetry_{};
};

static_assert(ente::domain::OperationalDomainConcept<FieldStationDomain>,
    "FieldStationDomain must strictly satisfy OperationalDomainConcept");

} // namespace fieldstation
