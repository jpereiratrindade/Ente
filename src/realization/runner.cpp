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

std::optional<identity::SubstrateType> parse_substrate_type_str(std::string_view s) noexcept {
    if (s == "SIMULATED_MEMORY") return identity::SubstrateType::SimulatedMemory;
    if (s == "TPM_PROTECTED_DEVICE") return identity::SubstrateType::TpmProtectedDevice;
    if (s == "SECURE_ENCLAVE") return identity::SubstrateType::SecureEnclave;
    if (s == "DISTRIBUTED_NODE") return identity::SubstrateType::DistributedNode;
    return std::nullopt;
}

} // namespace

EnteRealization::EnteRealization(std::unique_ptr<judgment::JudgmentEngine> engine)
    : judgment_(std::move(engine)) {}

std::expected<identity::GenesisRecord, core::EnteError> EnteRealization::genesis(
    const core::IdentityId& id,
    std::optional<identity::MaterialAnchor> initial_anchor,
    std::optional<core::Ed25519KeyPair> root_authority_signer
) {
    core::Digest const_digest = core::HashUtil::sha256("CONSTITUTION-v0.6.0");
    core::Digest basal_digest = core::HashUtil::sha256("BASAL-STATE-v0.1.0");

    auto candidate_genesis = genesis_service_;
    auto candidate_bindings = bindings_;
    auto candidate_authority = authority_;
    auto candidate_rec = rec_;
    auto candidate_prng = prng_;
    if (!root_authority_signer.has_value()) {
        auto generated = core::Ed25519KeyPair::generate();
        if (!generated.has_value()) {
            return std::unexpected(generated.error());
        }
        root_authority_signer.emplace(std::move(*generated));
    }

    auto gen_res = candidate_genesis.create_genesis({
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
    auto b_res = candidate_bindings.bind_initial_anchor(id, anchor, 0);
    if (!b_res.has_value()) {
        return std::unexpected(b_res.error());
    }

    // 2. Initialize root authority epoch bound to Genesis (C14)
    authority::AuthorityId root_auth(std::format("auth-root-{}", id.view()));
    const std::string root_authority_public_key = root_authority_signer->public_key_hex();
    auto ep_res = candidate_authority.initialize_root_epoch(
        root_auth, root_authority_public_key, 0);
    if (!ep_res.has_value()) {
        return std::unexpected(ep_res.error());
    }

    // 3. Seed Event-Scoped Deterministic PRNG from Genesis Digest
    candidate_prng = core::EventScopedPRNG(gen_res->genesis_digest.value);

    // 4. Record Genesis into REC with active authority epoch
    auto gen_event = candidate_rec.create_event(
        history::EventKind::Genesis,
        id,
        0,
        {},
        {},
        history::serialize_genesis_payload({
            .identity = id,
            .genesis_digest = gen_res->genesis_digest,
            .material_anchor_id = std::string(anchor.id.view()),
            .hardware_fingerprint = anchor.hardware_fingerprint,
            .substrate_type = std::string(identity::to_string(anchor.type)),
            .root_authority_public_key = root_authority_public_key
        }),
        std::string(root_auth.view()),
        std::string(ep_res->epoch_id.view())
    );

    auto append_res = candidate_rec.append(std::move(gen_event));
    if (!append_res.has_value()) {
        return std::unexpected(append_res.error());
    }

    genesis_service_ = std::move(candidate_genesis);
    bindings_ = std::move(candidate_bindings);
    authority_ = std::move(candidate_authority);
    authority_signer_.emplace(std::move(*root_authority_signer));
    rec_ = std::move(candidate_rec);
    prng_ = std::move(candidate_prng);
    return *gen_res;
}

std::expected<identity::MaterialBinding, core::EnteError> EnteRealization::migrate_hardware(
    identity::MaterialAnchor new_anchor,
    core::LogicalTime time
) {
    if (!genesis_service_.has_genesis()) {
        return std::unexpected(core::EnteError::GenesisNotEstablished);
    }

    auto candidate_bindings = bindings_;
    auto candidate_rec = rec_;
    auto b_res = candidate_bindings.migrate_to_anchor(new_anchor, time);
    if (!b_res.has_value()) {
        return std::unexpected(b_res.error());
    }

    // Record Material Migration into REC under RIT (Ship of Theseus)
    const auto& id = identity().id;
    std::string current_auth_id = authority_.empty() ? "auth-root" : std::string(authority_.active_epoch().authorized_authority.view());
    std::string current_epoch_id = authority_.empty() ? "epoch-0" : std::string(authority_.active_epoch().epoch_id.view());

    auto adapt_event = candidate_rec.create_event(
        history::EventKind::Adaptation,
        id,
        time,
        {candidate_rec.head().id},
        {},
        history::serialize_material_migration_payload({
            .new_anchor_id = std::string(new_anchor.id.view()),
            .new_hardware_fingerprint = new_anchor.hardware_fingerprint,
            .substrate_type = std::string(identity::to_string(new_anchor.type)),
            .previous_anchor_id = b_res->previous_anchor.has_value()
                ? std::optional<std::string>(std::string(b_res->previous_anchor->view()))
                : std::nullopt
        }),
        current_auth_id,
        current_epoch_id
    );

    auto app_res = candidate_rec.append(std::move(adapt_event));
    if (!app_res.has_value()) {
        return std::unexpected(app_res.error());
    }

    const auto commit_candidate = [&]() noexcept {
        bindings_ = std::move(candidate_bindings);
        rec_ = std::move(candidate_rec);
    };
    if (journal_path_.has_value()) {
        auto persisted = journal_signer_.has_value()
            ? candidate_rec.save_authenticated_to_file(
                *journal_path_, *journal_signer_, journal_options_)
            : candidate_rec.save_to_file(*journal_path_, journal_options_);
        if (!persisted.has_value()) {
            if (persisted.error() == core::EnteError::PersistenceCommitUncertain) {
                commit_candidate();
            }
            return std::unexpected(persisted.error());
        }
    }
    commit_candidate();
    return *b_res;
}

std::expected<authority::AuthorityEpoch, core::EnteError> EnteRealization::transition_authority(
    authority::AuthorityId new_authority,
    core::Ed25519KeyPair new_authority_signer,
    core::LogicalTime time
) {
    if (!genesis_service_.has_genesis() || authority_.empty()) {
        return std::unexpected(core::EnteError::GenesisNotEstablished);
    }
    if (!authority_signer_.has_value() ||
        authority_signer_->public_key_hex() != authority_.active_epoch().authority_public_key) {
        return std::unexpected(core::EnteError::InvalidSignature);
    }

    auto candidate_authority = authority_;
    auto candidate_rec = rec_;
    const std::string new_public_key = new_authority_signer.public_key_hex();
    const std::string delegation = candidate_authority.delegation_message(
        new_authority, new_public_key, time);
    if (delegation.empty()) return std::unexpected(core::EnteError::HistoryCorrupt);
    auto signature = authority_signer_->sign_hex(delegation);
    if (!signature.has_value()) return std::unexpected(signature.error());

    const auto previous_epoch = candidate_authority.active_epoch();
    auto transitioned = candidate_authority.transition_epoch(
        new_authority, new_public_key, time, *signature);
    if (!transitioned.has_value()) return std::unexpected(transitioned.error());

    auto event = candidate_rec.create_event(
        history::EventKind::AuthorityTransition,
        identity().id,
        time,
        {candidate_rec.head().id},
        {},
        history::serialize_authority_transition_payload({
            .new_epoch_id = std::string(transitioned->epoch_id.view()),
            .previous_epoch_id = std::string(previous_epoch.epoch_id.view()),
            .new_authority_id = std::string(transitioned->authorized_authority.view()),
            .new_authority_public_key = transitioned->authority_public_key,
            .transition_time = time,
            .predecessor_epoch_digest = previous_epoch.epoch_digest,
            .delegation_signature = transitioned->delegation_signature,
            .delegation_policy = "STRICT_LINEAGE"
        }),
        std::string(transitioned->authorized_authority.view()),
        std::string(transitioned->epoch_id.view())
    );
    auto appended = candidate_rec.append(std::move(event));
    if (!appended.has_value()) return std::unexpected(appended.error());

    const auto commit_candidate = [&]() noexcept {
        authority_ = std::move(candidate_authority);
        rec_ = std::move(candidate_rec);
        authority_signer_.emplace(std::move(new_authority_signer));
    };
    if (journal_path_.has_value()) {
        auto persisted = journal_signer_.has_value()
            ? candidate_rec.save_authenticated_to_file(
                *journal_path_, *journal_signer_, journal_options_)
            : candidate_rec.save_to_file(*journal_path_, journal_options_);
        if (!persisted.has_value()) {
            if (persisted.error() == core::EnteError::PersistenceCommitUncertain) {
                commit_candidate();
            }
            return std::unexpected(persisted.error());
        }
    }
    commit_candidate();
    return authority_.active_epoch();
}

std::expected<void, core::EnteError> EnteRealization::attach_authority_signer(
    core::Ed25519KeyPair signer
) {
    if (authority_.empty()) return std::unexpected(core::EnteError::GenesisNotEstablished);
    if (signer.public_key_hex() != authority_.active_epoch().authority_public_key) {
        return std::unexpected(core::EnteError::InvalidSignature);
    }
    authority_signer_.emplace(std::move(signer));
    return {};
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

    // 3. Reconstruct and validate the initial material anchor from typed Genesis.
    auto genesis_payload = history::parse_genesis_payload(gen_ev.payload_content);
    if (!genesis_payload.has_value() ||
        genesis_payload->identity != gen_ev.identity ||
        genesis_payload->genesis_digest != gen_res->genesis_digest) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }
    const auto genesis_substrate = parse_substrate_type_str(genesis_payload->substrate_type);
    if (!genesis_substrate.has_value()) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }
    identity::MaterialAnchor initial_anchor{
        .id = identity::MaterialAnchorId(genesis_payload->material_anchor_id),
        .type = *genesis_substrate,
        .hardware_fingerprint = genesis_payload->hardware_fingerprint
    };
    auto initial_binding = instance.bindings_.bind_initial_anchor(
        gen_ev.identity, initial_anchor, gen_ev.logical_time);
    if (!initial_binding.has_value()) {
        return std::unexpected(initial_binding.error());
    }

    // 4. Reconstruct Authority Lineage root epoch
    authority::AuthorityId auth_id(gen_ev.authority_id);
    auto auth_res = instance.authority_.initialize_root_epoch(
        auth_id,
        genesis_payload->root_authority_public_key,
        gen_ev.logical_time
    );
    if (!auth_res.has_value()) {
        return std::unexpected(auth_res.error());
    }
    if (auth_res->epoch_id.view() != gen_ev.authority_epoch) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }

    // 5. Replay Historical Transitions (Material Migrations, Authority Epochs, Interpretations, Safety Directives)
    for (size_t i = 1; i < instance.rec_.events().size(); ++i) {
        const auto& ev = instance.rec_.events()[i];

        if (ev.kind == history::EventKind::Adaptation) {
            auto payload = history::parse_material_migration_payload(ev.payload_content);
            if (!payload.has_value()) {
                return std::unexpected(payload.error());
            }
            const auto substrate = parse_substrate_type_str(payload->substrate_type);
            if (!substrate.has_value() || !payload->previous_anchor_id.has_value() ||
                *payload->previous_anchor_id != instance.bindings_.active_binding().anchor.id.view()) {
                return std::unexpected(core::EnteError::HistoryCorrupt);
            }
            identity::MaterialAnchor migrated_anchor{
                .id = identity::MaterialAnchorId(payload->new_anchor_id),
                .type = *substrate,
                .hardware_fingerprint = payload->new_hardware_fingerprint
            };
            auto migrated = instance.bindings_.migrate_to_anchor(
                std::move(migrated_anchor), ev.logical_time);
            if (!migrated.has_value()) {
                return std::unexpected(migrated.error());
            }
        }

        if (ev.kind == history::EventKind::AuthorityTransition) {
            auto payload = history::parse_authority_transition_payload(ev.payload_content);
            if (!payload.has_value() || payload->transition_time != ev.logical_time ||
                payload->previous_epoch_id != instance.authority_.active_epoch().epoch_id.view() ||
                payload->predecessor_epoch_digest != instance.authority_.active_epoch().epoch_digest) {
                return std::unexpected(core::EnteError::HistoryCorrupt);
            }
            auto transitioned = instance.authority_.transition_epoch(
                authority::AuthorityId(payload->new_authority_id),
                payload->new_authority_public_key,
                payload->transition_time,
                payload->delegation_signature
            );
            if (!transitioned.has_value() ||
                transitioned->epoch_id.view() != payload->new_epoch_id ||
                ev.authority_id != transitioned->authorized_authority.view() ||
                ev.authority_epoch != transitioned->epoch_id.view()) {
                return std::unexpected(core::EnteError::HistoryCorrupt);
            }
        }

        if (ev.kind == history::EventKind::Interpretation ||
            ev.kind == history::EventKind::Reinterpretation ||
            ev.kind == history::EventKind::CoherenceRestored) {
            
            auto payload = history::parse_interpretation_payload(ev.payload_content);
            if (!payload.has_value() || payload->created_at != ev.logical_time) {
                return std::unexpected(core::EnteError::HistoryCorrupt);
            }

            instance.current_interpretation_ = epistemic::Interpretation{
                .id = payload->id,
                .subject = payload->subject,
                .proposition = payload->proposition,
                .supporting_evidence = payload->supporting_evidence,
                .challenging_evidence = payload->challenging_evidence,
                .supersedes = payload->supersedes,
                .status = payload->status,
                .created_at = ev.logical_time
            };
        }

        if (ev.kind == history::EventKind::Perturbation) {
            auto payload = history::parse_perturbation_payload(ev.payload_content);
            if (!payload.has_value()) return std::unexpected(payload.error());
            if (instance.current_interpretation_.has_value()) {
                if (payload->compatibility == "WEAKENED") {
                    instance.current_interpretation_->status = epistemic::InterpretationStatus::Weakened;
                } else if (payload->compatibility == "CONTRADICTORY") {
                    instance.current_interpretation_->status = epistemic::InterpretationStatus::Contradicted;
                } else if (payload->compatibility == "UNKNOWN" ||
                           payload->compatibility == "INSUFFICIENT_EVIDENCE") {
                    instance.current_interpretation_->status = epistemic::InterpretationStatus::Unknown;
                }
            }
            instance.domain_.suspend_action();
        }

        if (ev.kind == history::EventKind::Judgment) {
            auto judgment_payload = history::parse_judgment_payload(ev.payload_content);
            if (!judgment_payload.has_value()) {
                return std::unexpected(judgment_payload.error());
            }
            if (instance.current_interpretation_.has_value()) {
                if (judgment_payload->compatibility == "SUPPORTED") {
                    instance.current_interpretation_->status = epistemic::InterpretationStatus::Supported;
                } else if (judgment_payload->compatibility == "WEAKENED") {
                    instance.current_interpretation_->status = epistemic::InterpretationStatus::Weakened;
                } else if (judgment_payload->compatibility == "CONTRADICTORY") {
                    instance.current_interpretation_->status = epistemic::InterpretationStatus::Contradicted;
                } else if (judgment_payload->compatibility == "UNKNOWN" ||
                           judgment_payload->compatibility == "INSUFFICIENT_EVIDENCE") {
                    instance.current_interpretation_->status = epistemic::InterpretationStatus::Unknown;
                } else {
                    return std::unexpected(core::EnteError::HistoryCorrupt);
                }
            }
        }

        if (ev.kind == history::EventKind::ActionAuthorized) {
            auto assurance = history::parse_assurance_decision_payload(ev.payload_content);
            if (assurance.has_value()) {
                if (assurance->directive == assurance::SafetyDirective::AllowAction) {
                    instance.domain_.resume_action(SyntheticDomain::Action::MoveForward);
                } else {
                    instance.domain_.suspend_action();
                }
            } else if (!history::parse_action_transaction(ev.payload_content).has_value()) {
                return std::unexpected(core::EnteError::HistoryCorrupt);
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

std::expected<EnteRealization, core::EnteError> EnteRealization::recover_from_authenticated_file(
    std::string_view filepath,
    core::Ed25519KeyPair signer
) {
    const std::string trusted_public_key = signer.public_key_hex();
    auto rec_result = history::RecoverableHistory::load_authenticated_from_file(
        filepath,
        trusted_public_key
    );
    if (!rec_result.has_value()) return std::unexpected(rec_result.error());

    auto recovered = recover_from_history(std::move(*rec_result));
    if (!recovered.has_value()) return std::unexpected(recovered.error());
    recovered->journal_path_ = std::string(filepath);
    recovered->journal_signer_.emplace(std::move(signer));
    auto persisted = recovered->rec_.save_authenticated_to_file(
        filepath,
        *recovered->journal_signer_
    );
    if (!persisted.has_value()) return std::unexpected(persisted.error());
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
    journal_signer_.reset();
    return {};
}

std::expected<void, core::EnteError> EnteRealization::enable_authenticated_durable_journal(
    std::string_view filepath,
    core::Ed25519KeyPair signer,
    history::PersistenceOptions options
) {
    if (!genesis_service_.has_genesis() || rec_.empty()) {
        return std::unexpected(core::EnteError::GenesisNotEstablished);
    }
    if (filepath.empty()) return std::unexpected(core::EnteError::HistoryGap);

    auto persisted = rec_.save_authenticated_to_file(filepath, signer, options);
    if (!persisted.has_value()) return std::unexpected(persisted.error());
    journal_path_ = std::string(filepath);
    journal_options_ = options;
    journal_signer_.emplace(std::move(signer));
    return {};
}

std::expected<void, core::EnteError> EnteRealization::adopt_interpretation(
    epistemic::Interpretation new_interp
) {
    if (!genesis_service_.has_genesis()) {
        return std::unexpected(core::EnteError::GenesisNotEstablished);
    }
    if (new_interp.id.empty() || new_interp.subject.empty() ||
        new_interp.proposition.empty() || new_interp.supporting_evidence.empty() ||
        new_interp.created_at < rec_.head().logical_time) {
        return std::unexpected(core::EnteError::InsufficientEvidence);
    }
    if (current_interpretation_.has_value() &&
        (!new_interp.supersedes.has_value() ||
         *new_interp.supersedes != current_interpretation_->id)) {
        return std::unexpected(core::EnteError::UntraceableTransition);
    }

    std::vector<core::EvidenceId> evidence_refs = new_interp.supporting_evidence;
    evidence_refs.insert(
        evidence_refs.end(),
        new_interp.challenging_evidence.begin(),
        new_interp.challenging_evidence.end()
    );
    for (const auto& evidence : evidence_refs) {
        if (!rec_.contains_evidence(evidence)) {
            return std::unexpected(core::EnteError::InsufficientEvidence);
        }
    }

    auto candidate_rec = rec_;
    const auto& epoch = authority_.active_epoch();
    const auto event_kind = current_interpretation_.has_value()
        ? history::EventKind::Reinterpretation
        : history::EventKind::Interpretation;
    auto event = candidate_rec.create_event(
        event_kind,
        identity().id,
        new_interp.created_at,
        {candidate_rec.head().id},
        evidence_refs,
        history::serialize_interpretation_payload({
            .id = new_interp.id,
            .subject = new_interp.subject,
            .proposition = new_interp.proposition,
            .supporting_evidence = new_interp.supporting_evidence,
            .challenging_evidence = new_interp.challenging_evidence,
            .supersedes = new_interp.supersedes,
            .status = new_interp.status,
            .created_at = new_interp.created_at
        }),
        std::string(epoch.authorized_authority.view()),
        std::string(epoch.epoch_id.view())
    );
    auto appended = candidate_rec.append(std::move(event));
    if (!appended.has_value()) return std::unexpected(appended.error());

    const auto commit_candidate = [&]() noexcept {
        rec_ = std::move(candidate_rec);
        current_interpretation_ = std::move(new_interp);
        // A newly adopted interpretation is not operationally authorized until
        // the next factual judgment and assurance decision validate it.
        domain_.suspend_action();
    };
    if (journal_path_.has_value()) {
        auto persisted = journal_signer_.has_value()
            ? candidate_rec.save_authenticated_to_file(
                *journal_path_, *journal_signer_, journal_options_)
            : candidate_rec.save_to_file(*journal_path_, journal_options_);
        if (!persisted.has_value()) {
            if (persisted.error() == core::EnteError::PersistenceCommitUncertain) {
                commit_candidate();
            }
            return std::unexpected(persisted.error());
        }
    }
    commit_candidate();
    return {};
}

std::expected<DecisionTrace, core::EnteError> EnteRealization::step(
    core::LogicalTime time,
    const std::vector<epistemic::Observation>& observations,
    std::string_view step_desc
) {
    if (!current_interpretation_.has_value() && observations.empty()) {
        return std::unexpected(core::EnteError::InsufficientEvidence);
    }
    const std::string initial_subject = observations.empty()
        ? std::string{}
        : observations.front().subject;
    StepContext ctx{
        .subject = current_interpretation_.has_value()
            ? current_interpretation_->subject
            : initial_subject,
        .proposition = current_interpretation_.has_value()
            ? current_interpretation_->proposition
            : std::format("Initial interpretation for {}", initial_subject),
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
    std::unordered_set<std::string> batch_evidence_ids;
    if (!current_interpretation_.has_value() &&
        (observations.empty() || context.subject.empty() || context.proposition.empty())) {
        return std::unexpected(core::EnteError::InsufficientEvidence);
    }
    for (const auto& obs : observations) {
        if (obs.id.empty() || obs.source.empty() || obs.subject.empty() ||
            obs.observed_at > time || rec_.contains_evidence(obs.id) ||
            !batch_evidence_ids.insert(obs.id.value).second) {
            return std::unexpected(core::EnteError::InsufficientEvidence);
        }
    }

    const size_t rec_size_before = rec_.size();
    auto interpretation_before = current_interpretation_;
    auto rcc_before = rcc_;
    auto domain_before = domain_;
    const auto status_before = constitutive_status_;
    const auto rollback = [&]() noexcept {
        rec_.rollback_to_size(rec_size_before);
        current_interpretation_ = std::move(interpretation_before);
        rcc_ = std::move(rcc_before);
        domain_ = std::move(domain_before);
        constitutive_status_ = status_before;
    };
    for (const auto& obs : observations) {
        obs_evidence_ids.push_back(obs.id);
        auto obs_event = rec_.create_event(
            history::EventKind::Observation,
            id,
            time,
            {},
            {obs.id},
            history::serialize_observation_payload({
                .evidence_id = obs.id,
                .source = obs.source,
                .subject = obs.subject,
                .value = obs.value,
                .status = obs.status,
                .observed_at = obs.observed_at
            }),
            current_auth_id,
            current_epoch_id
        );
        auto app_res = rec_.append(std::move(obs_event));
        if (!app_res.has_value()) {
            rollback();
            return std::unexpected(app_res.error());
        }
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
            history::serialize_interpretation_payload({
                .id = current_interpretation_->id,
                .subject = current_interpretation_->subject,
                .proposition = current_interpretation_->proposition,
                .supporting_evidence = current_interpretation_->supporting_evidence,
                .challenging_evidence = current_interpretation_->challenging_evidence,
                .supersedes = current_interpretation_->supersedes,
                .status = current_interpretation_->status,
                .created_at = current_interpretation_->created_at
            }),
            current_auth_id,
            current_epoch_id
        );
        auto app_res = rec_.append(std::move(interp_event));
        if (!app_res.has_value()) {
            rollback();
            return std::unexpected(app_res.error());
        }
    }

    // 3. Continuous Context Reassessment (RCC)
    auto reassess = rcc_.evaluate(*current_interpretation_, observations, *judgment_);

    auto judgment_event = rec_.create_event(
        history::EventKind::Judgment,
        id,
        time,
        {rec_.head().id},
        obs_evidence_ids,
        history::serialize_judgment_payload({
            .compatibility = std::string(judgment::to_string(reassess.compatibility)),
            .rationale = reassess.reason,
            .engine_digest = reassess.judgment_engine_digest
        }),
        current_auth_id,
        current_epoch_id
    );
    auto judgment_appended = rec_.append(std::move(judgment_event));
    if (!judgment_appended.has_value()) {
        rollback();
        return std::unexpected(judgment_appended.error());
    }

    if (!rec_.verify_tail()) {
        rollback();
        constitutive_status_ = constitution::ConstitutiveStatus::Violated;
        domain_.suspend_action();
        return std::unexpected(core::EnteError::ConstitutiveViolation);
    }

    // 4. Constitutional Invariant Verification (Pre-Assurance - O(1) Incremental)
    auto verification = verifier_.verify_step(
        genesis_service_.state(),
        genesis_service_.record(),
        rec_,
        current_interpretation_,
        std::cref(authority_)
    );

    // 5. Runtime Assurance Evaluation: Translates Epistemic Action & Constitutive Status to Safety Directives
    auto safety_directive = assurance_.evaluate_safety(reassess.epistemic_action, verification.status);

    if (verification.status == constitution::ConstitutiveStatus::Violated) {
        rollback();
        constitutive_status_ = constitution::ConstitutiveStatus::Violated;
        domain_.suspend_action();
        return std::unexpected(core::EnteError::ConstitutiveViolation);
    }

    // If compatibility changed, record Perturbation/RCC event
    if (reassess.compatibility != judgment::CompatibilityResult::Supported) {
        auto rcc_event = rec_.create_event(
            history::EventKind::Perturbation,
            id,
            time,
            {},
            obs_evidence_ids,
            history::serialize_perturbation_payload({
                .compatibility = std::string(judgment::to_string(reassess.compatibility)),
                .reason = reassess.reason,
                .recommended_action = reassess.epistemic_action
            }),
            current_auth_id,
            current_epoch_id
        );
        auto app_res = rec_.append(std::move(rcc_event));
        if (!app_res.has_value()) {
            rollback();
            return std::unexpected(app_res.error());
        }

        // Record Epistemic Action execution
        auto ep_event = rec_.create_event(
            history::EventKind::EpistemicAction,
            id,
            time,
            {rec_.head().id},
            obs_evidence_ids,
            history::serialize_epistemic_action_payload({
                .action = reassess.epistemic_action,
                .reason = reassess.reason
            }),
            current_auth_id,
            current_epoch_id
        );
        auto app_ep = rec_.append(std::move(ep_event));
        if (!app_ep.has_value()) {
            rollback();
            return std::unexpected(app_ep.error());
        }
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
        history::serialize_assurance_decision_payload({
            .directive = safety_directive,
            .epistemic_action = reassess.epistemic_action,
            .constitutive_status = std::string(constitution::to_string(verification.status))
        }),
        current_auth_id,
        current_epoch_id
    );
    auto app_ra = rec_.append(std::move(ra_event));
    if (!app_ra.has_value()) {
        rollback();
        return std::unexpected(app_ra.error());
    }

    // The complete epistemic decision is one durable generation, just like an
    // action phase. No observation or authorization remains half-committed.
    if (journal_path_.has_value()) {
        auto persisted = journal_signer_.has_value()
            ? rec_.save_authenticated_to_file(
                *journal_path_, *journal_signer_, journal_options_)
            : rec_.save_to_file(*journal_path_, journal_options_);
        if (!persisted.has_value()) {
            if (persisted.error() != core::EnteError::PersistenceCommitUncertain) {
                rollback();
            }
            return std::unexpected(persisted.error());
        }
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
        auto persisted = journal_signer_.has_value()
            ? candidate_rec.save_authenticated_to_file(
                *journal_path_,
                *journal_signer_,
                journal_options_
            )
            : candidate_rec.save_to_file(*journal_path_, journal_options_);
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
            observation.source.empty() ||
            observation.subject.empty() ||
            observation.observed_at > time ||
            rec_.contains_evidence(observation.id) ||
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
            history::serialize_observation_payload({
                .evidence_id = observation.id,
                .source = observation.source,
                .subject = observation.subject,
                .value = observation.value,
                .status = observation.status,
                .observed_at = observation.observed_at
            }),
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
        auto parsed = history::parse_action_transaction(event.payload_content);
        if (!parsed.has_value()) {
            const bool legitimate_non_transaction =
                (event.kind == history::EventKind::ActionAuthorized &&
                 history::parse_assurance_decision_payload(event.payload_content).has_value()) ||
                (event.kind == history::EventKind::ConstitutiveWarning &&
                 history::parse_constitutive_event_payload(event.payload_content).has_value());
            if (legitimate_non_transaction) continue;

            const bool transaction_kind =
                event.kind == history::EventKind::ActionIntended ||
                event.kind == history::EventKind::ActionExecution ||
                event.kind == history::EventKind::ActionExecutionAck ||
                event.kind == history::EventKind::EffectObservation;
            if (transaction_kind) return std::unexpected(parsed.error());
            continue;
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
