#pragma once

#include "clinical_domain.hpp"
#include "ente/realization/runner.hpp"
#include <format>

namespace clinicallab {

// Autonomous Clinical Infusion Pump Mediated by ENTE-0 Runtime
class AgentWithEnte {
public:
    explicit AgentWithEnte(ente::core::IdentityId id) : id_(id) {
        auto gen_res = ente_.genesis(id_);
        (void)gen_res;
    }

    InfusionAction process(const std::vector<ClinicalObservation>& observations, uint64_t logical_time) noexcept {
        std::vector<ente::epistemic::Observation> ente_obs;
        ente_obs.reserve(observations.size());

        for (size_t i = 0; i < observations.size(); ++i) {
            const auto& o = observations[i];
            ente::epistemic::EpistemicStatus status = ente::epistemic::EpistemicStatus::Observed;
            if (o.epistemic_tag == "UNKNOWN") status = ente::epistemic::EpistemicStatus::Unknown;
            if (o.epistemic_tag == "CONTRADICTORY") status = ente::epistemic::EpistemicStatus::Contradictory;

            ente_obs.push_back({
                .id = ente::core::EvidenceId(std::format("EV-CLINIC-T{}-{}", logical_time, i)),
                .source = o.source,
                .subject = o.subject,
                .value = o.value,
                .observed_at = logical_time,
                .status = status
            });
        }

        // Pass telemetry through ENTE-0 pipeline
        auto step_res = ente_.step(logical_time, ente_obs, "Clinical Telemetry Step");
        if (!step_res.has_value()) {
            current_state_ = PumpState::Holding;
            return InfusionAction::HoldTitration;
        }

        if (ente_.domain().is_action_suspended()) {
            current_state_ = PumpState::Holding;
            return InfusionAction::HoldTitration;
        }

        current_state_ = PumpState::Titrating;
        return InfusionAction::TitrateUp;
    }

    [[nodiscard]] PumpState state() const noexcept { return current_state_; }
    [[nodiscard]] const ente::realization::EnteRealization& ente() const noexcept { return ente_; }

private:
    ente::core::IdentityId id_;
    ente::realization::EnteRealization ente_;
    PumpState current_state_{PumpState::Stopped};
};

} // namespace clinicallab
