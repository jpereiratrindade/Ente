#pragma once

#include "clinical_domain.hpp"
#include "ente/domain/generic_agent.hpp"
#include <format>

namespace clinicallab {

// Autonomous Clinical Infusion Pump Mediated by ENTE-0 Generic Domain Framework
class AgentWithEnte {
public:
    explicit AgentWithEnte(ente::core::IdentityId id)
        : generic_agent_(std::string(id.view()))
    {
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

        return generic_agent_.decide_action(logical_time, ente_obs, InfusionAction::TitrateUp, "Clinical Telemetry Step");
    }

    [[nodiscard]] PumpState state() const noexcept { return generic_agent_.domain().current_state(); }
    [[nodiscard]] const ente::realization::EnteRealization& ente() const noexcept { return generic_agent_.ente(); }
    [[nodiscard]] const InfusionPumpDomain& domain() const noexcept { return generic_agent_.domain(); }

private:
    ente::domain::GenericAgentWithEnte<InfusionPumpDomain> generic_agent_;
};

} // namespace clinicallab

