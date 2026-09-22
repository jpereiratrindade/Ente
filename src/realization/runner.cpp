#include "ente/realization/runner.hpp"
#include "ente/core/hash.hpp"
#include <format>

namespace ente::realization {

EnteRealization::EnteRealization(std::unique_ptr<judgment::JudgmentEngine> engine)
    : judgment_(std::move(engine)) {}

std::expected<identity::GenesisRecord, core::EnteError> EnteRealization::genesis(const core::IdentityId& id) {
    core::Digest const_digest = core::HashUtil::sha256("CONSTITUTION-v0.6.0");
    core::Digest basal_digest = core::HashUtil::sha256("BASAL-STATE-v0.1.0");

    auto gen_res = genesis_service_.create_genesis({
        .identity = id,
        .constitution_digest = const_digest,
        .basal_state_digest = basal_digest
    });

    if (!gen_res.has_value()) {
        return std::unexpected(gen_res.error());
    }

    // Record Genesis into REC
    auto gen_event = rec_.create_event(
        history::EventKind::Genesis,
        id,
        0,
        {},
        {},
        std::format("GENESIS:{}:{}", id.view(), gen_res->genesis_digest.value)
    );

    auto append_res = rec_.append(std::move(gen_event));
    if (!append_res.has_value()) {
        return std::unexpected(append_res.error());
    }

    return *gen_res;
}

void EnteRealization::adopt_interpretation(epistemic::Interpretation new_interp) {
    current_interpretation_ = std::move(new_interp);
}

std::expected<void, core::EnteError> EnteRealization::step(
    core::LogicalTime time,
    const std::vector<epistemic::Observation>& observations,
    [[maybe_unused]] std::string_view step_desc
) {
    if (!genesis_service_.has_genesis()) {
        return std::unexpected(core::EnteError::GenesisNotEstablished);
    }

    const auto& id = identity().id;

    // 1. Record observations into REC
    std::vector<core::EvidenceId> obs_evidence_ids;
    for (const auto& obs : observations) {
        obs_evidence_ids.push_back(obs.id);
        auto obs_event = rec_.create_event(
            history::EventKind::Observation,
            id,
            time,
            {},
            {obs.id},
            std::format("OBSERVE:{}:{}:{}", obs.subject, obs.value, to_string(obs.status))
        );
        auto app_res = rec_.append(std::move(obs_event));
        if (!app_res.has_value()) return app_res;
    }

    // 2. If no current interpretation, establish basal/initial interpretation
    if (!current_interpretation_.has_value()) {
        core::InterpretationId interp_id("I0000");
        current_interpretation_ = epistemic::Interpretation{
            .id = interp_id,
            .subject = "path_clear",
            .proposition = "Caminho desobstruído para avanço",
            .supporting_evidence = obs_evidence_ids,
            .challenging_evidence = {},
            .supersedes = std::nullopt,
            .status = epistemic::InterpretationStatus::Current,
            .created_at = time
        };

        auto interp_event = rec_.create_event(
            history::EventKind::Interpretation,
            id,
            time,
            {},
            obs_evidence_ids,
            std::format("INTERPRET:I0000:path_clear:{}", current_interpretation_->proposition)
        );
        auto app_res = rec_.append(std::move(interp_event));
        if (!app_res.has_value()) return app_res;

        domain_.resume_action(SyntheticDomain::Action::MoveForward);
        return {};
    }

    // 3. Continuous Context Reassessment (RCC)
    auto reassess = rcc_.evaluate(*current_interpretation_, observations, *judgment_);

    // If compatibility changed, record Perturbation/RCC event
    if (reassess.compatibility != judgment::CompatibilityResult::Supported) {
        auto rcc_event = rec_.create_event(
            history::EventKind::Perturbation,
            id,
            time,
            {},
            obs_evidence_ids,
            std::format("RCC:PERTURBATION:{}:{}", judgment::to_string(reassess.compatibility), reassess.reason)
        );
        auto app_res = rec_.append(std::move(rcc_event));
        if (!app_res.has_value()) return app_res;

        // 4. Epistemic Action execution
        if (reassess.epistemic_action == rcc::EpistemicAction::SuspendAction ||
            reassess.epistemic_action == rcc::EpistemicAction::SeekEvidence) {
            domain_.suspend_action();

            auto ep_event = rec_.create_event(
                history::EventKind::EpistemicAction,
                id,
                time,
                {rec_.head().id},
                obs_evidence_ids,
                reassess.epistemic_action == rcc::EpistemicAction::SuspendAction ? "ACTION:SUSPEND_ACTION" : "ACTION:SEEK_EVIDENCE"
            );
            auto app_ep = rec_.append(std::move(ep_event));
            if (!app_ep.has_value()) return app_ep;
        }
    } else {
        // Maintained
        if (domain_.is_action_suspended()) {
            domain_.resume_action(SyntheticDomain::Action::MoveForward);
        }
    }

    // 5. Constitutional Invariant enforcement
    auto verification = verify();
    if (verification.status == constitution::ConstitutiveStatus::Violated) {
        domain_.suspend_action();
        return std::unexpected(core::EnteError::ConstitutiveViolation);
    }

    return {};
}

constitution::VerificationReport EnteRealization::verify() const noexcept {
    return verifier_.verify(
        genesis_service_.state(),
        genesis_service_.record(),
        rec_,
        current_interpretation_
    );
}

ExecutionSummary ScenarioRunner::run_scenario(
    EnteRealization& ente,
    const std::vector<ScenarioStep>& scenario
) {
    ExecutionSummary summary;
    summary.success = true;

    for (const auto& step : scenario) {
        auto res = ente.step(step.time, step.observations, step.description);
        if (!res.has_value()) {
            summary.success = false;
            break;
        }
    }

    summary.total_events = ente.history().size();
    summary.action_suspended = ente.domain().is_action_suspended();
    summary.final_verification = ente.verify();

    return summary;
}

} // namespace ente::realization
