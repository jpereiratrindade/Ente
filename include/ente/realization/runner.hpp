#pragma once

#include "ente/identity/genesis_service.hpp"
#include "ente/history/rec.hpp"
#include "ente/epistemic/interpretation.hpp"
#include "ente/judgment/fixture.hpp"
#include "ente/rcc/reassessment.hpp"
#include "ente/constitution/verifier.hpp"
#include "ente/realization/domain.hpp"
#include <vector>
#include <memory>
#include <expected>

namespace ente::realization {

struct ScenarioStep {
    core::LogicalTime time;
    std::vector<epistemic::Observation> observations;
    std::string description;
};

struct ExecutionSummary {
    bool success{false};
    size_t total_events{0};
    bool rcc_triggered{false};
    bool action_suspended{false};
    bool coherence_restored{false};
    constitution::VerificationReport final_verification;
};

class EnteRealization {
public:
    explicit EnteRealization(std::unique_ptr<judgment::JudgmentEngine> engine = std::make_unique<judgment::FixtureJudgmentEngine>());

    [[nodiscard]] std::expected<identity::GenesisRecord, core::EnteError> genesis(const core::IdentityId& id);

    [[nodiscard]] std::expected<void, core::EnteError> step(
        core::LogicalTime time,
        const std::vector<epistemic::Observation>& observations,
        std::string_view step_desc
    );

    [[nodiscard]] constitution::VerificationReport verify() const noexcept;

    [[nodiscard]] const identity::IdentityState& identity() const noexcept { return genesis_service_.state(); }
    [[nodiscard]] const history::RecoverableHistory& history() const noexcept { return rec_; }
    [[nodiscard]] history::RecoverableHistory& history_mut() noexcept { return rec_; } // for tamper test
    [[nodiscard]] const std::optional<epistemic::Interpretation>& current_interpretation() const noexcept { return current_interpretation_; }
    [[nodiscard]] const SyntheticDomain& domain() const noexcept { return domain_; }

    void adopt_interpretation(epistemic::Interpretation new_interp);

private:
    identity::GenesisService genesis_service_;
    history::RecoverableHistory rec_;
    std::optional<epistemic::Interpretation> current_interpretation_;
    rcc::ContextReassessment rcc_;
    std::unique_ptr<judgment::JudgmentEngine> judgment_;
    constitution::ConstitutionVerifier verifier_;
    SyntheticDomain domain_;
};

class ScenarioRunner {
public:
    [[nodiscard]] static ExecutionSummary run_scenario(
        EnteRealization& ente,
        const std::vector<ScenarioStep>& scenario
    );
};

} // namespace ente::realization
