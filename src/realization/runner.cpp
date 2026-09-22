#include "ente/realization/runner.hpp"
#include "ente/core/hash.hpp"
#include <format>

namespace ente::realization {

EnteRealization::EnteRealization(std::unique_ptr<judgment::JudgmentEngine> engine)
    : judgment_(std::move(engine)) {}

std::expected<identity::GenesisRecord, core::EnteError> EnteRealization::genesis(
    const core::IdentityId& id,
    std::optional<identity::MaterialAnchor> initial_anchor
) {
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

    // 1. Bind Initial Material Anchor (SimulatedMemory or Hardware TPM)
    identity::MaterialAnchor anchor = initial_anchor.value_or(identity::MaterialAnchor{
        .id = identity::MaterialAnchorId("anchor-genesis-0"),
        .type = identity::SubstrateType::SimulatedMemory,
        .hardware_fingerprint = "fp-simulated-root-memory"
    });
    auto b_res = bindings_.bind_initial_anchor(id, anchor, 0);
    if (!b_res.has_value()) {
        return std::unexpected(b_res.error());
    }

    // 2. Initialize root authority epoch bound to Genesis (C14)
    authority::AuthorityId root_auth(std::format("auth-root-{}", id.view()));
    auto ep_res = authority_.initialize_root_epoch(root_auth, 0);
    if (!ep_res.has_value()) {
        return std::unexpected(ep_res.error());
    }

    // 3. Record Genesis into REC with active authority epoch
    auto gen_event = rec_.create_event(
        history::EventKind::Genesis,
        id,
        0,
        {},
        {},
        std::format("GENESIS:{}:{}:{}", id.view(), gen_res->genesis_digest.value, anchor.id.view()),
        std::string(root_auth.view()),
        std::string(ep_res->epoch_id.view())
    );

    auto append_res = rec_.append(std::move(gen_event));
    if (!append_res.has_value()) {
        return std::unexpected(append_res.error());
    }

    return *gen_res;
}

std::expected<identity::MaterialBinding, core::EnteError> EnteRealization::migrate_hardware(
    identity::MaterialAnchor new_anchor,
    core::LogicalTime time
) {
    if (!genesis_service_.has_genesis()) {
        return std::unexpected(core::EnteError::GenesisNotEstablished);
    }

    auto b_res = bindings_.migrate_to_anchor(new_anchor, time);
    if (!b_res.has_value()) {
        return std::unexpected(b_res.error());
    }

    // Record Material Migration into REC under RIT
    const auto& id = identity().id;
    std::string current_auth_id = authority_.empty() ? "auth-root" : std::string(authority_.active_epoch().authorized_authority.view());
    std::string current_epoch_id = authority_.empty() ? "epoch-0" : std::string(authority_.active_epoch().epoch_id.view());

    auto adapt_event = rec_.create_event(
        history::EventKind::Adaptation,
        id,
        time,
        {rec_.head().id},
        {},
        std::format("MIGRATE_HARDWARE:{}:{}", new_anchor.id.view(), new_anchor.hardware_fingerprint),
        current_auth_id,
        current_epoch_id
    );

    auto app_res = rec_.append(std::move(adapt_event));
    if (!app_res.has_value()) {
        return std::unexpected(app_res.error());
    }

    return *b_res;
}

std::expected<EnteRealization, core::EnteError> EnteRealization::recover_from_history(history::RecoverableHistory history) {
    if (history.empty()) {
        return std::unexpected(core::EnteError::GenesisNotEstablished);
    }

    if (!history.verify_integrity()) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }

    const auto& gen_ev = history.events()[0];
    if (gen_ev.kind != history::EventKind::Genesis) {
        return std::unexpected(core::EnteError::GenesisNotEstablished);
    }

    EnteRealization instance;
    instance.rec_ = std::move(history);

    // Reconstruct Genesis record into GenesisService to seal identity and prevent 2nd genesis
    core::Digest const_digest = core::HashUtil::sha256("CONSTITUTION-v0.6.0");
    core::Digest basal_digest = core::HashUtil::sha256("BASAL-STATE-v0.1.0");
    auto gen_res = instance.genesis_service_.create_genesis({
        .identity = gen_ev.identity,
        .constitution_digest = const_digest,
        .basal_state_digest = basal_digest
    });
    if (!gen_res.has_value()) {
        return std::unexpected(gen_res.error());
    }

    // Reconstruct Initial Material Binding
    identity::MaterialAnchor rec_anchor{
        .id = identity::MaterialAnchorId("anchor-recovered-0"),
        .type = identity::SubstrateType::SimulatedMemory,
        .hardware_fingerprint = "fp-recovered"
    };
    (void)instance.bindings_.bind_initial_anchor(gen_ev.identity, rec_anchor, gen_ev.logical_time);

    // Reconstruct Authority Lineage from events
    authority::AuthorityId auth_id(gen_ev.authority_id);
    auto auth_res = instance.authority_.initialize_root_epoch(auth_id, gen_ev.logical_time);
    if (!auth_res.has_value()) {
        return std::unexpected(auth_res.error());
    }

    // Reconstruct latest interpretation from event history if present
    for (auto it = instance.rec_.events().rbegin(); it != instance.rec_.events().rend(); ++it) {
        if (it->kind == history::EventKind::Interpretation ||
            it->kind == history::EventKind::Reinterpretation ||
            it->kind == history::EventKind::CoherenceRestored) {
            
            instance.current_interpretation_ = epistemic::Interpretation{
                .id = core::InterpretationId("I_RECOVERED"),
                .subject = "recovered_context",
                .proposition = it->payload_content,
                .supporting_evidence = it->evidence_refs,
                .challenging_evidence = {},
                .supersedes = std::nullopt,
                .status = epistemic::InterpretationStatus::Current,
                .created_at = it->logical_time
            };
            break;
        }
    }

    return instance;
}

std::expected<EnteRealization, core::EnteError> EnteRealization::recover_from_file(std::string_view filepath) {
    auto rec_res = history::RecoverableHistory::load_from_file(filepath);
    if (!rec_res.has_value()) {
        return std::unexpected(rec_res.error());
    }
    return recover_from_history(std::move(*rec_res));
}

void EnteRealization::adopt_interpretation(epistemic::Interpretation new_interp) {
    current_interpretation_ = std::move(new_interp);

    // Runtime Assurance & Action Support Trace
    if (current_interpretation_->subject != "path_clear" ||
        current_interpretation_->status == epistemic::InterpretationStatus::Weakened ||
        current_interpretation_->status == epistemic::InterpretationStatus::Contradicted) {
        domain_.suspend_action();
    }
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
    std::string current_auth_id = authority_.empty() ? "auth-root" : std::string(authority_.active_epoch().authorized_authority.view());
    std::string current_epoch_id = authority_.empty() ? "epoch-0" : std::string(authority_.active_epoch().epoch_id.view());

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
            std::format("OBSERVE:{}:{}:{}", obs.subject, obs.value, to_string(obs.status)),
            current_auth_id,
            current_epoch_id
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
            std::format("INTERPRET:I0000:path_clear:{}", current_interpretation_->proposition),
            current_auth_id,
            current_epoch_id
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
            std::format("RCC:PERTURBATION:{}:{}", judgment::to_string(reassess.compatibility), reassess.reason),
            current_auth_id,
            current_epoch_id
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
                reassess.epistemic_action == rcc::EpistemicAction::SuspendAction ? "ACTION:SUSPEND_ACTION" : "ACTION:SEEK_EVIDENCE",
                current_auth_id,
                current_epoch_id
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
