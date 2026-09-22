#pragma once

#include "ente/identity/genesis_service.hpp"
#include "ente/identity/material_anchor.hpp"
#include "ente/authority/authority.hpp"
#include "ente/history/rec.hpp"
#include "ente/epistemic/interpretation.hpp"
#include "ente/judgment/fixture.hpp"
#include "ente/rcc/reassessment.hpp"
#include "ente/assurance/runtime_assurance.hpp"
#include "ente/constitution/verifier.hpp"
#include "ente/realization/domain.hpp"
#include <vector>
#include <memory>
#include <expected>

#include "ente/core/prng.hpp"

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

    [[nodiscard]] std::expected<identity::GenesisRecord, core::EnteError> genesis(
        const core::IdentityId& id,
        std::optional<identity::MaterialAnchor> initial_anchor = std::nullopt
    );

    // Hardware replacement under RIT (Ship of Theseus: S0 -> S1) preserving Identity
    [[nodiscard]] std::expected<identity::MaterialBinding, core::EnteError> migrate_hardware(
        identity::MaterialAnchor new_anchor,
        core::LogicalTime time
    );

    // Cold Recovery from historical REC (restores identity, authority, material bindings, interpretation, and blocks 2nd Genesis)
    [[nodiscard]] static std::expected<EnteRealization, core::EnteError> recover_from_history(history::RecoverableHistory history);
    [[nodiscard]] static std::expected<EnteRealization, core::EnteError> recover_from_file(std::string_view filepath);

    [[nodiscard]] std::expected<void, core::EnteError> step(
        core::LogicalTime time,
        const std::vector<epistemic::Observation>& observations,
        std::string_view step_desc
    );

    [[nodiscard]] constitution::VerificationReport verify() const noexcept;

    [[nodiscard]] const identity::IdentityState& identity() const noexcept { return genesis_service_.state(); }
    [[nodiscard]] const identity::MaterialBindingRegistry& material_bindings() const noexcept { return bindings_; }
    [[nodiscard]] const history::RecoverableHistory& history() const noexcept { return rec_; }
    [[nodiscard]] history::RecoverableHistory& history_mut() noexcept { return rec_; } // for tamper test
    [[nodiscard]] const std::optional<epistemic::Interpretation>& current_interpretation() const noexcept { return current_interpretation_; }
    [[nodiscard]] const SyntheticDomain& domain() const noexcept { return domain_; }
    [[nodiscard]] const authority::AuthorityLineage& authority_lineage() const noexcept { return authority_; }
    [[nodiscard]] const core::EventScopedPRNG& prng() const noexcept { return prng_; }

    void adopt_interpretation(epistemic::Interpretation new_interp);

private:
    identity::GenesisService genesis_service_;
    identity::MaterialBindingRegistry bindings_;
    authority::AuthorityLineage authority_;
    history::RecoverableHistory rec_;
    core::EventScopedPRNG prng_;
    std::optional<epistemic::Interpretation> current_interpretation_;
    rcc::ContextReassessment rcc_;
    assurance::RuntimeAssurance assurance_;
    std::unique_ptr<judgment::JudgmentEngine> judgment_;
    constitution::ConstitutionVerifier verifier_;
    SyntheticDomain domain_;
    mutable constitution::ConstitutiveStatus constitutive_status_{constitution::ConstitutiveStatus::Valid};
};

class ScenarioRunner {
public:
    [[nodiscard]] static ExecutionSummary run_scenario(
        EnteRealization& ente,
        const std::vector<ScenarioStep>& scenario
    );
};

} // namespace ente::realization
