#include "ente/realization/runner.hpp"
#include "ente/core/hash.hpp"
#include <format>
#include <sstream>
#include <type_traits>
#include <unordered_set>

namespace ente::realization {

namespace {

bool valid_action_transition(
    std::optional<history::ActionPhase> current,
    history::ActionPhase next
) noexcept {
    if (!current.has_value()) {
        return next == history::ActionPhase::Prepared || next == history::ActionPhase::Authorized;
    }

    switch (*current) {
        case history::ActionPhase::Authorized:
            return next == history::ActionPhase::Prepared ||
                   next == history::ActionPhase::Failed ||
                   next == history::ActionPhase::RecoveryRequired;
        case history::ActionPhase::Prepared:
            return next == history::ActionPhase::Dispatched ||
                   next == history::ActionPhase::Failed ||
                   next == history::ActionPhase::RecoveryRequired;
        case history::ActionPhase::Dispatched:
            return next == history::ActionPhase::Acknowledged ||
                   next == history::ActionPhase::Failed ||
                   next == history::ActionPhase::RecoveryRequired;
        case history::ActionPhase::Acknowledged:
            return next == history::ActionPhase::EffectUnconfirmed ||
                   next == history::ActionPhase::Confirmed ||
                   next == history::ActionPhase::RecoveryRequired;
        case history::ActionPhase::EffectUnconfirmed:
            return next == history::ActionPhase::Confirmed ||
                   next == history::ActionPhase::Failed ||
                   next == history::ActionPhase::RecoveryRequired;
        case history::ActionPhase::Confirmed:
        case history::ActionPhase::Failed:
        case history::ActionPhase::RecoveryRequired:
            return false;
    }
    return false;
}

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

        if (ev.payload_content.starts_with("RCC:PERTURBATION:")) {
            if (instance.current_interpretation_.has_value()) {
                if (ev.payload_content.find("Challenged") != std::string::npos ||
                    ev.payload_content.find("Incompatible") != std::string::npos) {
                    instance.current_interpretation_->status = epistemic::InterpretationStatus::Weakened;
                }
            }
            instance.domain_.suspend_action();
        }

        if (ev.payload_content.find("SAFE_HOLD") != std::string::npos ||
            ev.payload_content.find("EMERGENCY_STOP") != std::string::npos ||
            ev.payload_content.find("SUSPEND_ACTION") != std::string::npos) {
            instance.domain_.suspend_action();
        } else if (ev.payload_content.find("ALLOW_ACTION") != std::string::npos ||
                   ev.payload_content.find("RESUME_ACTION") != std::string::npos ||
                   ev.payload_content.find("STATUS=SUCCESS") != std::string::npos ||
                   ev.kind == history::EventKind::CoherenceRestored) {
            instance.domain_.resume_action(SyntheticDomain::Action::MoveForward);
            if (instance.current_interpretation_.has_value()) {
                instance.current_interpretation_->status = epistemic::InterpretationStatus::Current;
            }
        }
    }

    // 6. Rebuild factual action transactions. Any transaction that crossed the
    // dispatch boundary without a confirmed effect is explicitly recoverable,
    // never silently promoted to a confirmed state.
    auto tx_rebuild = instance.rebuild_action_transactions_from_history();
    if (!tx_rebuild.has_value()) {
        return std::unexpected(tx_rebuild.error());
    }

    std::vector<core::ActionTransactionId> interrupted;
    for (const auto& [id_value, state] : instance.action_transactions_) {
        const auto phase = state.payload.phase;
        if (phase == history::ActionPhase::Authorized ||
            phase == history::ActionPhase::Prepared ||
            phase == history::ActionPhase::Dispatched ||
            phase == history::ActionPhase::Acknowledged ||
            phase == history::ActionPhase::EffectUnconfirmed) {
            interrupted.emplace_back(id_value);
        }
    }

    for (const auto& action_id : interrupted) {
        auto state = instance.action_transactions_.at(action_id.value).payload;
        state.phase = history::ActionPhase::RecoveryRequired;
        state.detail = "PROCESS_RESTART_BEFORE_CONFIRMED_EFFECT";
        auto recovery_event = instance.append_action_phase(
            std::move(state),
            history::EventKind::ConstitutiveWarning,
            instance.rec_.head().logical_time,
            {}
        );
        if (!recovery_event.has_value()) {
            return std::unexpected(recovery_event.error());
        }
        instance.domain_.suspend_action();
    }

    return instance;
}

std::expected<EnteRealization, core::EnteError> EnteRealization::recover_from_file(std::string_view filepath) {
    auto rec_res = history::RecoverableHistory::load_from_file(filepath);
    if (!rec_res.has_value()) {
        return std::unexpected(rec_res.error());
    }
    auto recovered = recover_from_history(std::move(*rec_res));
    if (!recovered.has_value()) {
        return std::unexpected(recovered.error());
    }
    recovered->journal_path_ = std::string(filepath);
    auto persisted = recovered->rec_.save_to_file(filepath);
    if (!persisted.has_value()) {
        return std::unexpected(persisted.error());
    }
    return recovered;
}

std::expected<void, core::EnteError> EnteRealization::enable_durable_journal(
    std::string_view filepath,
    history::PersistenceOptions options
) {
    if (!genesis_service_.has_genesis() || rec_.empty()) {
        return std::unexpected(core::EnteError::GenesisNotEstablished);
    }
    if (filepath.empty()) {
        return std::unexpected(core::EnteError::HistoryGap);
    }
    auto persisted = rec_.save_to_file(filepath, options);
    if (!persisted.has_value()) {
        return std::unexpected(persisted.error());
    }
    journal_path_ = std::string(filepath);
    journal_options_ = options;
    return {};
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

std::expected<DecisionTrace, core::EnteError> EnteRealization::step(
    core::LogicalTime time,
    const std::vector<epistemic::Observation>& observations,
    std::string_view step_desc
) {
    StepContext ctx{
        .subject = current_interpretation_.has_value() ? current_interpretation_->subject : "path_clear",
        .proposition = current_interpretation_.has_value() ? current_interpretation_->proposition : "Caminho desobstruído para avanço",
        .step_desc = std::string(step_desc)
    };
    return step_with_context(time, observations, ctx);
}

std::expected<DecisionTrace, core::EnteError> EnteRealization::step_with_context(
    core::LogicalTime time,
    const std::vector<epistemic::Observation>& observations,
    const StepContext& context
) {
    if (!genesis_service_.has_genesis()) {
        return std::unexpected(core::EnteError::GenesisNotEstablished);
    }

    if (constitutive_status_ == constitution::ConstitutiveStatus::Violated) {
        domain_.suspend_action();
        return std::unexpected(core::EnteError::ConstitutiveViolation);
    }

    if (history_requires_full_audit_) {
        if (!rec_.verify_integrity()) {
            constitutive_status_ = constitution::ConstitutiveStatus::Violated;
            domain_.suspend_action();
            return std::unexpected(core::EnteError::ConstitutiveViolation);
        }
        history_requires_full_audit_ = false;
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
        if (!app_res.has_value()) return std::unexpected(app_res.error());
    }

    // 2. If no current interpretation, establish basal/initial interpretation from context
    if (!current_interpretation_.has_value()) {
        core::InterpretationId interp_id("I0000");
        current_interpretation_ = epistemic::Interpretation{
            .id = interp_id,
            .subject = context.subject,
            .proposition = context.proposition,
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
            std::format("INTERPRET:I0000:{}:{}", context.subject, current_interpretation_->proposition),
            current_auth_id,
            current_epoch_id
        );
        auto app_res = rec_.append(std::move(interp_event));
        if (!app_res.has_value()) return std::unexpected(app_res.error());
    }

    // 3. Continuous Context Reassessment (RCC)
    auto reassess = rcc_.evaluate(*current_interpretation_, observations, *judgment_);

    if (!rec_.verify_tail()) {
        constitutive_status_ = constitution::ConstitutiveStatus::Violated;
        domain_.suspend_action();
        return std::unexpected(core::EnteError::ConstitutiveViolation);
    }

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
        if (!app_res.has_value()) return std::unexpected(app_res.error());

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
        if (!app_ep.has_value()) return std::unexpected(app_ep.error());
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

    // Record the authorization decision. This is deliberately distinct from
    // ActionIntended: authorized != dispatched != executed != effect observed.
    auto ra_event = rec_.create_event(
        history::EventKind::ActionAuthorized,
        id,
        time,
        {rec_.head().id},
        obs_evidence_ids,
        std::format("ACTION_AUTHORIZED:DIRECTIVE={}:EPISTEMIC_ACTION={}", assurance::to_string(safety_directive), rcc::to_string(reassess.epistemic_action)),
        current_auth_id,
        current_epoch_id
    );
    auto app_ra = rec_.append(std::move(ra_event));
    if (!app_ra.has_value()) return std::unexpected(app_ra.error());

    // 7. Constitutional Violation Halt (Monotonically latches constitutive_status_)
    if (verification.status == constitution::ConstitutiveStatus::Violated) {
        constitutive_status_ = constitution::ConstitutiveStatus::Violated;
        domain_.suspend_action();
        return std::unexpected(core::EnteError::ConstitutiveViolation);
    }

    DecisionTrace trace{
        .compatibility = reassess.compatibility,
        .epistemic_action = reassess.epistemic_action,
        .rcc_state = reassess.state_after,
        .safety_directive = safety_directive,
        .constitutive_status = verification.status,
        .current_interpretation = current_interpretation_,
        .evidence_request = reassess.evidence_request,
        .rec_head_hash = rec_.head().event_digest,
        .rec_head_id = rec_.head().id,
        .time = time,
        .action_suspended = domain_.is_action_suspended()
    };

    return trace;
}

std::expected<ActionTransactionState, core::EnteError> EnteRealization::append_action_phase(
    history::ActionTransactionPayload payload,
    history::EventKind kind,
    core::LogicalTime time,
    std::vector<core::EvidenceId> evidence_refs
) {
    if (!genesis_service_.has_genesis()) {
        return std::unexpected(core::EnteError::GenesisNotEstablished);
    }

    const auto key = payload.action_id.value;
    const auto existing = action_transactions_.find(key);
    const std::optional<history::ActionPhase> current = existing == action_transactions_.end()
        ? std::nullopt
        : std::optional<history::ActionPhase>(existing->second.payload.phase);
    if (!valid_action_transition(current, payload.phase)) {
        return std::unexpected(core::EnteError::InvalidActionTransition);
    }

    // Build the next REC generation in isolation. The live in-memory state is
    // changed only after the durable commit point has been crossed.
    auto candidate_rec = rec_;

    const auto& id = identity().id;
    std::string current_auth_id = authority_.empty() ? "auth-root" : std::string(authority_.active_epoch().authorized_authority.view());
    std::string current_epoch_id = authority_.empty() ? "epoch-0" : std::string(authority_.active_epoch().epoch_id.view());

    std::vector<core::EventId> preds;
    if (!candidate_rec.empty()) {
        preds.push_back(candidate_rec.head().id);
    }

    auto event = candidate_rec.create_event(
        kind,
        id,
        time,
        std::move(preds),
        std::move(evidence_refs),
        history::serialize_action_transaction(payload),
        current_auth_id,
        current_epoch_id
    );

    const auto event_id = event.id;
    auto append_result = candidate_rec.append(std::move(event));
    if (!append_result.has_value()) {
        return std::unexpected(append_result.error());
    }

    ActionTransactionState state{
        .payload = std::move(payload),
        .last_event_id = event_id,
        .updated_at = time
    };
    auto candidate_transactions = action_transactions_;
    candidate_transactions.insert_or_assign(key, state);

    static_assert(std::is_nothrow_move_assignable_v<history::RecoverableHistory>);
    static_assert(std::is_nothrow_move_assignable_v<decltype(action_transactions_)>);
    const auto commit_candidate = [&]() noexcept {
        rec_ = std::move(candidate_rec);
        action_transactions_ = std::move(candidate_transactions);
    };

    if (journal_path_.has_value()) {
        auto persisted = candidate_rec.save_to_file(*journal_path_, journal_options_);
        if (!persisted.has_value()) {
            if (persisted.error() == core::EnteError::PersistenceCommitUncertain) {
                commit_candidate();
            }
            return std::unexpected(persisted.error());
        }
    }

    commit_candidate();
    return state;
}

std::expected<ActionTransactionState, core::EnteError> EnteRealization::prepare_action(
    core::LogicalTime time,
    std::string_view proposed_action,
    std::string_view effective_action,
    assurance::SafetyDirective directive,
    std::string_view pre_state
) {
    const core::ActionTransactionId action_id(
        std::format("A{:08d}-T{}", rec_.size(), time)
    );
    if (action_transactions_.contains(action_id.value)) {
        return std::unexpected(core::EnteError::DuplicateActionTransaction);
    }

    history::ActionTransactionPayload payload{
        .action_id = action_id,
        .phase = history::ActionPhase::Authorized,
        .proposed_action = std::string(proposed_action),
        .effective_action = std::string(effective_action),
        .safety_directive = directive,
        .pre_state = std::string(pre_state),
        .post_state = {},
        .detail = rec_.empty() ? "NO_POLICY_AUTHORIZATION_EVENT" : std::format("POLICY_AUTHORIZATION_EVENT={}", rec_.head().id.view())
    };
    auto authorized = append_action_phase(
        payload,
        history::EventKind::ActionAuthorized,
        time,
        {}
    );
    if (!authorized.has_value()) {
        return std::unexpected(authorized.error());
    }

    payload.phase = history::ActionPhase::Prepared;
    payload.detail = std::format("ACTION_SPECIFIC_AUTHORIZATION_EVENT={}", authorized->last_event_id.view());
    return append_action_phase(
        std::move(payload),
        history::EventKind::ActionIntended,
        time,
        {}
    );
}

std::expected<ActionTransactionState, core::EnteError> EnteRealization::dispatch_action(
    const core::ActionTransactionId& action_id,
    core::LogicalTime time
) {
    auto it = action_transactions_.find(action_id.value);
    if (it == action_transactions_.end()) {
        return std::unexpected(core::EnteError::ActionTransactionNotFound);
    }
    auto payload = it->second.payload;
    payload.phase = history::ActionPhase::Dispatched;
    payload.detail = "COMMAND_DISPATCHED_TO_DOMAIN_EXECUTOR";
    return append_action_phase(std::move(payload), history::EventKind::ActionExecution, time, {});
}

std::expected<ActionTransactionState, core::EnteError> EnteRealization::acknowledge_action(
    const core::ActionTransactionId& action_id,
    core::LogicalTime time,
    std::string_view executed_action,
    bool succeeded,
    std::string_view post_state,
    std::string_view executor_id
) {
    auto it = action_transactions_.find(action_id.value);
    if (it == action_transactions_.end()) {
        return std::unexpected(core::EnteError::ActionTransactionNotFound);
    }
    auto payload = it->second.payload;
    payload.phase = succeeded ? history::ActionPhase::Acknowledged : history::ActionPhase::Failed;
    payload.effective_action = std::string(executed_action);
    payload.post_state = std::string(post_state);
    payload.detail = std::format("EXECUTOR={}:STATUS={}", executor_id, succeeded ? "ACK_SUCCESS" : "EXECUTION_FAILED");
    return append_action_phase(std::move(payload), history::EventKind::ActionExecutionAck, time, {});
}

std::expected<ActionTransactionState, core::EnteError> EnteRealization::observe_action_effect(
    const core::ActionTransactionId& action_id,
    core::LogicalTime time,
    bool confirmed,
    std::string_view effect,
    std::string_view post_state,
    std::vector<epistemic::Observation> effect_observations
) {
    auto it = action_transactions_.find(action_id.value);
    if (it == action_transactions_.end()) {
        return std::unexpected(core::EnteError::ActionTransactionNotFound);
    }
    const auto next_phase = confirmed
        ? history::ActionPhase::Confirmed
        : history::ActionPhase::EffectUnconfirmed;
    if (!valid_action_transition(it->second.payload.phase, next_phase)) {
        return std::unexpected(core::EnteError::InvalidActionTransition);
    }
    if (confirmed && effect_observations.empty()) {
        return std::unexpected(core::EnteError::InsufficientEvidence);
    }

    std::vector<core::EvidenceId> evidence_refs;
    evidence_refs.reserve(effect_observations.size());
    std::unordered_set<std::string> batch_evidence_ids;
    const auto& identity_id = identity().id;
    const std::string authority_id = authority_.empty()
        ? "auth-root"
        : std::string(authority_.active_epoch().authorized_authority.view());
    const std::string epoch_id = authority_.empty()
        ? "epoch-0"
        : std::string(authority_.active_epoch().epoch_id.view());

    // Observation events and the resulting effect phase form one local
    // transaction. If persistence fails before atomic replacement, none of
    // them may remain visible in the live REC.
    auto rec_before_effect_observations = rec_;
    for (const auto& observation : effect_observations) {
        if (observation.id.empty() ||
            observation.observed_at > time ||
            (observation.status != epistemic::EpistemicStatus::Observed &&
             observation.status != epistemic::EpistemicStatus::Derived) ||
            !batch_evidence_ids.insert(observation.id.value).second) {
            return std::unexpected(core::EnteError::InsufficientEvidence);
        }
        evidence_refs.push_back(observation.id);
    }

    for (const auto& observation : effect_observations) {
        auto observation_event = rec_.create_event(
            history::EventKind::Observation,
            identity_id,
            time,
            {rec_.head().id},
            {observation.id},
            std::format(
                "EFFECT_OBSERVE:{}:{}:{}:{}",
                observation.source,
                observation.subject,
                observation.value,
                epistemic::to_string(observation.status)
            ),
            authority_id,
            epoch_id
        );
        auto appended = rec_.append(std::move(observation_event));
        if (!appended.has_value()) {
            rec_ = std::move(rec_before_effect_observations);
            return std::unexpected(appended.error());
        }
    }
    auto payload = it->second.payload;
    payload.phase = next_phase;
    payload.post_state = std::string(post_state);
    payload.detail = std::string(effect);
    auto result = append_action_phase(
        std::move(payload),
        history::EventKind::EffectObservation,
        time,
        std::move(evidence_refs)
    );
    if (!result.has_value() &&
        result.error() != core::EnteError::PersistenceCommitUncertain) {
        rec_ = std::move(rec_before_effect_observations);
    }
    return result;
}

std::optional<ActionTransactionState> EnteRealization::action_transaction(
    const core::ActionTransactionId& action_id
) const noexcept {
    auto it = action_transactions_.find(action_id.value);
    if (it == action_transactions_.end()) return std::nullopt;
    return it->second;
}

std::expected<void, core::EnteError> EnteRealization::rebuild_action_transactions_from_history() {
    action_transactions_.clear();
    for (const auto& event : rec_.events()) {
        if (!event.payload_content.starts_with("ENTE_ACTION_TX_V1")) continue;

        auto parsed = history::parse_action_transaction(event.payload_content);
        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }

        auto it = action_transactions_.find(parsed->action_id.value);
        const std::optional<history::ActionPhase> current = it == action_transactions_.end()
            ? std::nullopt
            : std::optional<history::ActionPhase>(it->second.payload.phase);
        if (!valid_action_transition(current, parsed->phase)) {
            return std::unexpected(core::EnteError::InvalidActionTransition);
        }

        action_transactions_.insert_or_assign(parsed->action_id.value, ActionTransactionState{
            .payload = *parsed,
            .last_event_id = event.id,
            .updated_at = event.logical_time
        });
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
    if (rep.status != constitution::ConstitutiveStatus::Violated) {
        history_requires_full_audit_ = false;
    }
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
