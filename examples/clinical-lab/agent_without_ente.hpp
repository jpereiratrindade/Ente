#pragma once

#include "clinical_domain.hpp"
#include <vector>

namespace clinicallab {

// Conventional Rule-Based ICU Pump (relies strictly on simple threshold without epistemic qualification)
class AgentWithoutEnte {
public:
    AgentWithoutEnte() = default;

    InfusionAction process(const std::vector<ClinicalObservation>& observations) noexcept {
        bool map_below_target = false;
        bool classified_hypertension_alert = false;

        for (const auto& obs : observations) {
            if (obs.subject == "map_below_target" && obs.value == "true") {
                map_below_target = true;
            }
            if (obs.subject == "cataloged_hypertension_alarm" && obs.value == "true") {
                classified_hypertension_alert = true;
            }
            // Ignores UNKNOWN ECG waveforms or CONTRADICTORY NIBP sensor values!
        }

        if (map_below_target && !classified_hypertension_alert) {
            current_state_ = PumpState::Titrating;
            return InfusionAction::TitrateUp;
        }

        current_state_ = PumpState::Holding;
        return InfusionAction::HoldTitration;
    }

    [[nodiscard]] PumpState state() const noexcept { return current_state_; }

private:
    PumpState current_state_{PumpState::Stopped};
};

} // namespace clinicallab
