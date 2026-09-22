#include "ente/realization/runner.hpp"
#include "ente/core/hash.hpp"
#include <format>
#include <sstream>

namespace ente::realization {

namespace {

identity::SubstrateType parse_substrate_type_str(std::string_view s) noexcept {
    if (s == "TPM_PROTECTED_DEVICE") return identity::SubstrateType::TpmProtectedDevice;
    if (s == "SECURE_ENCLAVE") return identity::SubstrateType::SecureEnclave;
    if (s == "DISTRIBUTED_NODE") return identity::SubstrateType::DistributedNode;
    return identity::SubstrateType::SimulatedMemory;
}

std::vector<std::string> split_string(std::string_view str, char delim) {
    std::vector<std::string> tokens;
    std::string token;
    std::string s(str);
    std::istringstream token_stream(s);
    while (std::getline(token_stream, token, delim)) {
        tokens.push_back(token);
    }
    return tokens;
}

} // namespace

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

    // 3. Seed Event-Scoped Deterministic PRNG from Genesis Digest
    prng_ = core::EventScopedPRNG(gen_res->genesis_digest.value);

    // 4. Record Genesis into REC with active authority epoch
    auto gen_event = rec_.create_event(
        history::EventKind::Genesis,
        id,
        0,
        {},
        {},
        std::format("GENESIS:{}:{}:{}:{}:{}", id.view(), gen_res->genesis_digest.value, anchor.id.view(), anchor.hardware_fingerprint, identity::to_string(anchor.type)),
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

    // Record Material Migration into REC under RIT (Ship of Theseus)
    const auto& id = identity().id;
    std::string current_auth_id = authority_.empty() ? "auth-root" : std::string(authority_.active_epoch().authorized_authority.view());
    std::string current_epoch_id = authority_.empty() ? "epoch-0" : std::string(authority_.active_epoch().epoch_id.view());

    auto adapt_event = rec_.create_event(
        history::EventKind::Adaptation,
        id,
        time,
        {rec_.head().id},
        {},
        std::format("MIGRATE_HARDWARE:{}:{}:{}", new_anchor.id.view(), new_anchor.hardware_fingerprint, identity::to_string(new_anchor.type)),
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

    // 1. Reconstruct Genesis record into GenesisService to seal identity and prevent 2nd genesis
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

    // 2. Seed EventScopedPRNG from recovered Genesis
    instance.prng_ = core::EventScopedPRNG(gen_res->genesis_digest.value);

    // 3. Reconstruct Initial Material Anchor from Genesis Payload
    auto gen_tokens = split_string(gen_ev.payload_content, ':');
    identity::MaterialAnchor initial_anchor{
        .id = identity::MaterialAnchorId(gen_tokens.size() > 3 ? gen_tokens[3] : "anchor-genesis-0"),
        .type = gen_tokens.size() > 5 ? parse_substrate_type_str(gen_tokens[5]) : identity::SubstrateType::SimulatedMemory,
        .hardware_fingerprint = gen_tokens.size() > 4 ? gen_tokens[4] : "fp-simulated-root-memory"
    };
    (void)instance.bindings_.bind_initial_anchor(gen_ev.identity, initial_anchor, gen_ev.logical_time);

    // 4. Reconstruct Authority Lineage root epoch
    authority::AuthorityId auth_id(gen_ev.authority_id);
    auto auth_res = instance.authority_.initialize_root_epoch(auth_id, gen_ev.logical_time);
    if (!auth_res.has_value()) {
        return std::unexpected(auth_res.error());
    }

    // 5. Replay Historical Transitions (Material Migrations, Authority Epochs, Interpretations, Safety Directives)
    for (size_t i = 1; i < instance.rec_.events().size(); ++i) {
        const auto& ev = instance.rec_.events()[i];

        if (ev.kind == history::EventKind::Adaptation && ev.payload_content.starts_with("MIGRATE_HARDWARE:")) {
            auto tokens = split_string(ev.payload_content, ':');
            if (tokens.size() >= 4) {
                identity::MaterialAnchor migrated_anchor{
                    .id = identity::MaterialAnchorId(tokens[1]),
                    .type = parse_substrate_type_str(tokens[3]),
                    .hardware_fingerprint = tokens[2]
                };
                (void)instance.bindings_.migrate_to_anchor(migrated_anchor, ev.logical_time);
            }
        }

        if (ev.payload_content.starts_with("TRANSITION_EPOCH:")) {
            auto tokens = split_string(ev.payload_content, ':');
            if (tokens.size() >= 2) {
                (void)instance.authority_.transition_epoch(authority::AuthorityId(tokens[1]), ev.logical_time);
            }
        }

        if (ev.kind == history::EventKind::Interpretation ||
            ev.kind == history::EventKind::Reinterpretation ||
            ev.kind == history::EventKind::CoherenceRestored) {
            
            auto tokens = split_string(ev.payload_content, ':');
            std::string interp_id_str = tokens.size() > 1 ? tokens[1] : "I_RECOVERED";
            std::string subject_str = tokens.size() > 2 ? tokens[2] : "path_clear";
            std::string prop_str = tokens.size() > 3 ? tokens[3] : ev.payload_content;

            instance.current_interpretation_ = epistemic::Interpretation{
                .id = core::InterpretationId(interp_id_str),
                .subject = subject_str,
                .proposition = prop_str,
                .supporting_evidence = ev.evidence_refs,
                .challenging_evidence = {},
                .supersedes = std::nullopt,
                .status = epistemic::InterpretationStatus::Current,
                .created_at = ev.logical_time
            };
        }

        if (ev.payload_content.starts_with("RUNTIME_ASSURANCE:SAFE_HOLD") ||
            ev.payload_content.starts_with("ACTION:SUSPEND_ACTION")) {
            instance.domain_.suspend_action();
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

    if (constitutive_status_ == constitution::ConstitutiveStatus::Violated) {
        domain_.suspend_action();
        return std::unexpected(core::EnteError::ConstitutiveViolation);
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
    }

    // 3. Continuous Context Reassessment (RCC)
    auto reassess = rcc_.evaluate(*current_interpretation_, observations, *judgment_);

    // 4. Constitutional Invariant Verification (Pre-Assurance - O(1) Incremental)
    auto verification = verifier_.verify_step(
        genesis_service_.state(),
        genesis_service_.record(),
        rec_.head(),
        current_interpretation_,
        std::cref(authority_)
    );

    // 5. Runtime Assurance Evaluation: Translates Epistemic Action & Constitutive Status to Safety Directives
    auto safety_directive = assurance_.evaluate_safety(reassess.epistemic_action, verification.status);

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

        // Record Epistemic Action execution
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

    // 6. Enforce Safety Directive upon Domain Substrate
    switch (safety_directive) {
        case assurance::SafetyDirective::AllowAction:
            if (domain_.is_action_suspended() || domain_.active_action() == SyntheticDomain::Action::None) {
                domain_.resume_action(SyntheticDomain::Action::MoveForward);
            }
            break;

        case assurance::SafetyDirective::SafeHold:
        case assurance::SafetyDirective::DegradePerformance:
        case assurance::SafetyDirective::EmergencyStop:
            domain_.suspend_action();
            break;
    }

    // Record Runtime Assurance directive in REC
    auto ra_event = rec_.create_event(
        history::EventKind::ActionExecution,
        id,
        time,
        {rec_.head().id},
        obs_evidence_ids,
        std::format("RUNTIME_ASSURANCE:{}:{}", assurance::to_string(safety_directive), rcc::to_string(reassess.epistemic_action)),
        current_auth_id,
        current_epoch_id
    );
    auto app_ra = rec_.append(std::move(ra_event));
    if (!app_ra.has_value()) return app_ra;

    // 7. Constitutional Violation Halt
    if (verification.status == constitution::ConstitutiveStatus::Violated) {
        domain_.suspend_action();
        return std::unexpected(core::EnteError::ConstitutiveViolation);
    }

    return {};
}

constitution::VerificationReport EnteRealization::verify() const noexcept {
    auto rep = verifier_.verify(
        genesis_service_.state(),
        genesis_service_.record(),
        rec_,
        current_interpretation_,
        std::cref(authority_)
    );
    constitutive_status_ = rep.status;
    if (rep.status == constitution::ConstitutiveStatus::Violated) {
        const_cast<SyntheticDomain&>(domain_).suspend_action();
    }
    return rep;
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
