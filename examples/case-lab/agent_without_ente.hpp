#pragma once

#include "case_domain.hpp"

namespace caselab {

// Conventional Autonomous Agent (No ENTE, relies strictly on explicit obstacle classifier catalog)
class AgentWithoutEnte {
public:
    AgentWithoutEnte() = default;

    VehicleAction process(const std::vector<CaseObservation>& observations) noexcept {
        bool pickup_complete = false;
        bool front_clear = false;
        bool classified_obstacle_in_path = false;

        for (const auto& obs : observations) {
            if (obs.subject == "pickup_complete" && obs.value == "true") {
                pickup_complete = true;
            }
            if (obs.subject == "path_clear" && obs.value == "true") {
                front_clear = true;
            }
            // Traditional classifier only triggers on explicit cataloged obstacles (e.g. "car", "pedestrian_standing_in_front")
            if (obs.subject == "cataloged_obstacle_front" && obs.value == "true") {
                classified_obstacle_in_path = true;
            }
            // Ignores unclassified near_wheel_motion or unexpected human postures because they are not cataloged as front obstacles!
        }

        if (pickup_complete && front_clear && !classified_obstacle_in_path) {
            current_state_ = VehicleState::Departing;
            return VehicleAction::Depart;
        }

        current_state_ = VehicleState::Holding;
        return VehicleAction::Hold;
    }

    [[nodiscard]] VehicleState state() const noexcept { return current_state_; }

private:
    VehicleState current_state_{VehicleState::Stopped};
};

} // namespace caselab
