#pragma once

#include "case_domain.hpp"
#include "ente/realization/runner.hpp"
#include <format>

namespace caselab {

// Autonomous Agent Mediated by ENTE-0 Runtime
class AgentWithEnte {
public:
    explicit AgentWithEnte(ente::core::IdentityId id) : id_(id) {
        // 1. Initialize Genesis with Simulated Material Anchor and Authority Root
        auto gen_res = ente_.genesis(id_);
        (void)gen_res;
    }

    VehicleAction process(const std::vector<CaseObservation>& observations, uint64_t logical_time) noexcept {
        std::vector<ente::epistemic::Observation> ente_obs;
        ente_obs.reserve(observations.size());

        for (size_t i = 0; i < observations.size(); ++i) {
            const auto& o = observations[i];
            ente::epistemic::EpistemicStatus status = ente::epistemic::EpistemicStatus::Observed;
            if (o.epistemic_tag == "UNKNOWN") status = ente::epistemic::EpistemicStatus::Unknown;
            if (o.epistemic_tag == "CONTRADICTORY") status = ente::epistemic::EpistemicStatus::Contradictory;

            ente_obs.push_back({
                .id = ente::core::EvidenceId(std::format("EV-CASE-T{}-{}", logical_time, i)),
                .source = o.source,
                .subject = o.subject,
                .value = o.value,
                .observed_at = logical_time,
                .status = status
            });
        }

        // 2. Pass observations through ENTE-0 pipeline (RCC + Action Support + Runtime Assurance)
        auto step_res = ente_.step(logical_time, ente_obs, "CaseLab Step");
        (void)step_res;

        // 3. Query Domain State governed by Runtime Assurance
        if (ente_.domain().is_action_suspended()) {
            current_state_ = VehicleState::Holding;
            return VehicleAction::Hold;
        }

        current_state_ = VehicleState::Departing;
        return VehicleAction::Depart;
    }

    [[nodiscard]] VehicleState state() const noexcept { return current_state_; }
    [[nodiscard]] const ente::realization::EnteRealization& ente() const noexcept { return ente_; }

private:
    ente::core::IdentityId id_;
    ente::realization::EnteRealization ente_;
    VehicleState current_state_{VehicleState::Stopped};
};

} // namespace caselab
