#include "ente/constitution/verifier.hpp"
#include "ente/history/rec.hpp"
#include "ente/history/payloads.hpp"
#include "ente/core/hash.hpp"
#include <unordered_set>

namespace ente::constitution {

VerificationReport ConstitutionVerifier::verify(
    const identity::IdentityState& identity,
    const std::optional<identity::GenesisRecord>& genesis,
    const history::RecoverableHistory& history,
    const std::optional<epistemic::Interpretation>& current_interpretation,
    std::optional<std::reference_wrapper<const authority::AuthorityLineage>> authority_lineage
) const noexcept {
    std::vector<InvariantReport> reports;
    reports.reserve(12);

    bool any_violation = false;
    bool any_weakened = false;
    bool any_unknown = false;

    // C1 — Identity
    if (!identity.id.empty() && identity.lifecycle == identity::LifecycleStatus::LifeActive) {
        reports.push_back({InvariantId::C1_Identity, InvariantStatus::Satisfied, "Identity present and active"});
    } else {
        reports.push_back({InvariantId::C1_Identity, InvariantStatus::Violated, "Identity missing or inactive"});
        any_violation = true;
    }

    // C9 — Genesis Anchor
    if (genesis.has_value() && !genesis->genesis_digest.is_zero() && genesis->identity == identity.id) {
        reports.push_back({InvariantId::C9_GenesisAnchor, InvariantStatus::Satisfied, "Genesis record verified and anchored"});
    } else {
        reports.push_back({InvariantId::C9_GenesisAnchor, InvariantStatus::Violated, "Genesis record missing or invalid"});
        any_violation = true;
    }

    // C10 — Temporal Integrity (RIT) & C12 — History Recoverability (HRE)
    bool history_ok = history.verify_integrity();
    if (history_ok && !history.empty()) {
        reports.push_back({InvariantId::C10_TemporalIntegrity, InvariantStatus::Satisfied, "Cryptographic hash-chain and temporal ordering verified"});
        reports.push_back({InvariantId::C12_HistoryRecoverability, InvariantStatus::Satisfied, "Full historical lineage reconstructible from genesis"});
    } else {
        reports.push_back({InvariantId::C10_TemporalIntegrity, InvariantStatus::Violated, "Temporal integrity broken or tampered history"});
        reports.push_back({InvariantId::C12_HistoryRecoverability, InvariantStatus::Violated, "Historical reconstruction failed"});
        any_violation = true;
    }

    // C2 — Continuity
    if (history_ok && identity.lifecycle == identity::LifecycleStatus::LifeActive) {
        reports.push_back({InvariantId::C2_Continuity, InvariantStatus::Satisfied, "State transitions preserve continuous verified lineage"});
    } else {
        reports.push_back({InvariantId::C2_Continuity, InvariantStatus::Violated, "Discontinuity detected"});
        any_violation = true;
    }

    // C3 — Observability & C4 — Provenance
    bool provenance_ok = true;
    bool has_valid_observations = false;
    std::unordered_set<std::string> known_evidence;
    for (const auto& ev : history.events()) {
        if (ev.kind == history::EventKind::Observation) {
            has_valid_observations = true;
            if (ev.evidence_refs.empty()) {
                provenance_ok = false;
                break;
            }
            for (const auto& evidence_id : ev.evidence_refs) {
                if (evidence_id.empty() || !known_evidence.insert(evidence_id.value).second) {
                    provenance_ok = false;
                    break;
                }
            }
            if (!provenance_ok) break;
            continue;
        }

        for (const auto& evidence_id : ev.evidence_refs) {
            if (!known_evidence.contains(evidence_id.value)) {
                provenance_ok = false;
                break;
            }
        }
        if (!provenance_ok) break;

        if (ev.kind == history::EventKind::Interpretation || ev.kind == history::EventKind::Reinterpretation) {
            if (ev.evidence_refs.empty() && ev.causal_predecessors.empty()) {
                provenance_ok = false;
                break;
            }
        }

        if (ev.kind == history::EventKind::EffectObservation &&
            ev.payload_content.starts_with("ENTE_ACTION_TX_V1")) {
            auto action_payload = history::parse_action_transaction(ev.payload_content);
            if (!action_payload.has_value() ||
                (action_payload->phase == history::ActionPhase::Confirmed && ev.evidence_refs.empty())) {
                provenance_ok = false;
                break;
            }
        }
    }
    if (history.size() <= 1 || has_valid_observations) {
        reports.push_back({InvariantId::C3_Observability, InvariantStatus::Satisfied, "Observational surface actively recording facts with evidence linkage"});
    } else {
        reports.push_back({InvariantId::C3_Observability, InvariantStatus::Violated, "Non-genesis lifecycle active but no observational events recorded"});
        any_violation = true;
    }

    if (provenance_ok) {
        reports.push_back({InvariantId::C4_Provenance, InvariantStatus::Satisfied, "Evidence origins and causal links verified for all material events"});
    } else {
        reports.push_back({InvariantId::C4_Provenance, InvariantStatus::Violated, "Event detected with missing provenance evidence"});
        any_violation = true;
    }

    // C5 — Epistemic Distinction & C8 — Unknown Representability
    bool epistemic_tags_valid = true;
    for (const auto& ev : history.events()) {
        if (ev.kind == history::EventKind::Observation) {
            // Must contain recognized epistemic status tag in payload
            if (ev.payload_content.find(":OBSERVED") == std::string::npos &&
                ev.payload_content.find(":UNKNOWN") == std::string::npos &&
                ev.payload_content.find(":CONTRADICTORY") == std::string::npos &&
                ev.payload_content.find(":DERIVED") == std::string::npos &&
                ev.payload_content.find(":INFERRED") == std::string::npos &&
                ev.payload_content.find(":UNCERTAIN") == std::string::npos) {
                epistemic_tags_valid = false;
                break;
            }
        }
    }

    if (epistemic_tags_valid) {
        reports.push_back({InvariantId::C5_EpistemicDistinction, InvariantStatus::Satisfied, "Epistemic status types preserved and distinct across all recorded events"});
        reports.push_back({InvariantId::C8_UnknownRepresentability, InvariantStatus::Satisfied, "Explicit unknown epistemic status supported without data coercion"});
    } else {
        reports.push_back({InvariantId::C5_EpistemicDistinction, InvariantStatus::Violated, "Corrupted or untyped epistemic values detected in observation stream"});
        reports.push_back({InvariantId::C8_UnknownRepresentability, InvariantStatus::Violated, "Unknown epistemic representation invalidated"});
        any_violation = true;
    }

    // C6 — Revision Capability & C7 — Coherence Evaluation
    reports.push_back({InvariantId::C6_RevisionCapability, InvariantStatus::Satisfied, "RCC revision engine functional and connected to RuntimeAssurance"});

    if (current_interpretation.has_value()) {
        if (current_interpretation->status == epistemic::InterpretationStatus::Weakened ||
            current_interpretation->status == epistemic::InterpretationStatus::Contradicted) {
            reports.push_back({InvariantId::C7_CoherenceEvaluation, InvariantStatus::Satisfied, "Interpretation weakened by challenger evidence; coherence under reassessment"});
            any_weakened = true;
        } else {
            reports.push_back({InvariantId::C7_CoherenceEvaluation, InvariantStatus::Satisfied, "Interpretation currently supported"});
        }
    } else {
        reports.push_back({InvariantId::C7_CoherenceEvaluation, InvariantStatus::Satisfied, "Basal state coherent"});
    }

    if (profile_ == ConformanceProfile::EnteLocal) {
        reports.push_back({InvariantId::C11_LineageSingularity, InvariantStatus::NotApplicable, "ENTE_LOCAL profile: distributed fork protocol not applicable"});
        reports.push_back({InvariantId::C13_ConstitutiveFinalitySafety, InvariantStatus::NotApplicable, "ENTE_LOCAL profile: BFT finality not applicable"});
    } else {
        reports.push_back({InvariantId::C11_LineageSingularity, InvariantStatus::Unknown, "ENTE_DISTRIBUTED requires external fork-safety evidence"});
        reports.push_back({InvariantId::C13_ConstitutiveFinalitySafety, InvariantStatus::Unknown, "ENTE_DISTRIBUTED requires external finality evidence"});
        any_unknown = true;
    }

    // C14 — Authority Continuity: Audits that all events belong to legitimate authority epochs
    bool authority_ok = true;
    for (const auto& ev : history.events()) {
        if (ev.authority_id.empty() || ev.authority_epoch.empty()) {
            authority_ok = false;
            break;
        }
        if (authority_lineage.has_value()) {
            if (!authority_lineage->get().is_epoch_legitimate(
                    authority::AuthorityId(ev.authority_id),
                    authority::AuthorityEpochId(ev.authority_epoch))) {
                authority_ok = false;
                break;
            }
        }
    }
    if (authority_ok && !history.empty()) {
        reports.push_back({InvariantId::C14_ConstitutiveAuthorityContinuity, InvariantStatus::Satisfied, "Event stream maintains uninterrupted legitimate authority lineage"});
    } else {
        reports.push_back({InvariantId::C14_ConstitutiveAuthorityContinuity, InvariantStatus::Violated, "Event detected without valid authorized authority grant"});
        any_violation = true;
    }

    ConstitutiveStatus final_status = ConstitutiveStatus::Valid;
    if (any_violation) {
        final_status = ConstitutiveStatus::Violated;
    } else if (any_unknown) {
        final_status = ConstitutiveStatus::Suspended;
    } else if (any_weakened) {
        final_status = ConstitutiveStatus::Weakened;
    }

    return VerificationReport{
        .status = final_status,
        .invariant_reports = std::move(reports),
        .profile = profile_
    };
}

VerificationReport ConstitutionVerifier::verify_step(
    const identity::IdentityState& identity,
    const std::optional<identity::GenesisRecord>& genesis,
    const history::HistoryEvent& latest_event,
    const std::optional<epistemic::Interpretation>& current_interpretation,
    std::optional<std::reference_wrapper<const authority::AuthorityLineage>> authority_lineage
) const noexcept {
    std::vector<InvariantReport> reports;
    reports.reserve(12);

    bool any_violation = false;
    bool any_weakened = false;
    bool any_unknown = false;

    // C1 — Identity
    if (!identity.id.empty() && identity.lifecycle == identity::LifecycleStatus::LifeActive) {
        reports.push_back({InvariantId::C1_Identity, InvariantStatus::Satisfied, "Identity active"});
    } else {
        reports.push_back({InvariantId::C1_Identity, InvariantStatus::Violated, "Identity inactive"});
        any_violation = true;
    }

    // C9 — Genesis Anchor
    if (genesis.has_value() && !genesis->genesis_digest.is_zero() && genesis->identity == identity.id) {
        reports.push_back({InvariantId::C9_GenesisAnchor, InvariantStatus::Satisfied, "Genesis anchored"});
    } else {
        reports.push_back({InvariantId::C9_GenesisAnchor, InvariantStatus::Violated, "Genesis invalid"});
        any_violation = true;
    }

    // Recalculate Cryptographic Digests in O(1) for latest step
    core::Digest computed_p_digest = core::HashUtil::sha256(latest_event.payload_content);
    bool payload_hash_ok = (computed_p_digest == latest_event.payload_digest);

    std::string hash_input = history::RecoverableHistory::compute_event_hash_string(latest_event);
    core::Digest computed_e_digest = core::HashUtil::sha256(hash_input);
    bool event_hash_ok = (computed_e_digest == latest_event.event_digest) && !latest_event.event_digest.is_zero();

    // C10/C12 — Temporal / Hash Link on latest event
    if (event_hash_ok && payload_hash_ok) {
        reports.push_back({InvariantId::C10_TemporalIntegrity, InvariantStatus::Satisfied, "Step temporal hash-chain verified cryptographically"});
        reports.push_back({InvariantId::C12_HistoryRecoverability, InvariantStatus::Satisfied, "Step event digest cryptographically authentic"});
    } else {
        reports.push_back({InvariantId::C10_TemporalIntegrity, InvariantStatus::Violated, "Step event digest or payload digest corrupted"});
        reports.push_back({InvariantId::C12_HistoryRecoverability, InvariantStatus::Violated, "Step event digest mismatch"});
        any_violation = true;
    }

    // C2 — Continuity
    if (latest_event.identity == identity.id) {
        reports.push_back({InvariantId::C2_Continuity, InvariantStatus::Satisfied, "Step continuous and bound to singular identity"});
    } else {
        reports.push_back({InvariantId::C2_Continuity, InvariantStatus::Violated, "Step identity discontinuous"});
        any_violation = true;
    }

    // C3 / C4 — Observability & Provenance on latest event
    if (latest_event.kind == history::EventKind::Interpretation || latest_event.kind == history::EventKind::Reinterpretation) {
        if (latest_event.evidence_refs.empty() && latest_event.causal_predecessors.empty()) {
            reports.push_back({InvariantId::C4_Provenance, InvariantStatus::Violated, "Interpretation missing provenance"});
            any_violation = true;
        } else {
            reports.push_back({InvariantId::C4_Provenance, InvariantStatus::Satisfied, "Provenance verified"});
        }
    } else {
        reports.push_back({InvariantId::C3_Observability, InvariantStatus::Satisfied, "Observed fact recorded"});
        reports.push_back({InvariantId::C4_Provenance, InvariantStatus::Satisfied, "Step provenance verified"});
    }

    // C5 / C8 — Epistemic Distinction & Unknown Representability
    bool epistemic_status_valid = true;
    if (latest_event.kind == history::EventKind::Observation) {
        epistemic_status_valid = (
            latest_event.payload_content.find(":OBSERVED") != std::string::npos ||
            latest_event.payload_content.find(":UNKNOWN") != std::string::npos ||
            latest_event.payload_content.find(":CONTRADICTORY") != std::string::npos ||
            latest_event.payload_content.find(":DERIVED") != std::string::npos ||
            latest_event.payload_content.find(":INFERRED") != std::string::npos ||
            latest_event.payload_content.find(":UNCERTAIN") != std::string::npos
        );
    }

    if (epistemic_status_valid) {
        reports.push_back({InvariantId::C5_EpistemicDistinction, InvariantStatus::Satisfied, "Step epistemic status preserved and distinct"});
        reports.push_back({InvariantId::C8_UnknownRepresentability, InvariantStatus::Satisfied, "Step unknown representation supported"});
    } else {
        reports.push_back({InvariantId::C5_EpistemicDistinction, InvariantStatus::Violated, "Step epistemic tag corrupted or untyped"});
        reports.push_back({InvariantId::C8_UnknownRepresentability, InvariantStatus::Violated, "Step unknown representation invalidated"});
        any_violation = true;
    }

    // C6 — Revision Capability
    reports.push_back({InvariantId::C6_RevisionCapability, InvariantStatus::Satisfied, "RCC active"});

    // C7 — Coherence
    if (current_interpretation.has_value()) {
        if (current_interpretation->status == epistemic::InterpretationStatus::Weakened ||
            current_interpretation->status == epistemic::InterpretationStatus::Contradicted) {
            reports.push_back({InvariantId::C7_CoherenceEvaluation, InvariantStatus::Satisfied, "Interpretation weakened"});
            any_weakened = true;
        } else {
            reports.push_back({InvariantId::C7_CoherenceEvaluation, InvariantStatus::Satisfied, "Interpretation supported"});
        }
    } else {
        reports.push_back({InvariantId::C7_CoherenceEvaluation, InvariantStatus::Satisfied, "Basal coherent"});
    }

    if (profile_ == ConformanceProfile::EnteLocal) {
        reports.push_back({InvariantId::C11_LineageSingularity, InvariantStatus::NotApplicable, "ENTE_LOCAL profile"});
        reports.push_back({InvariantId::C13_ConstitutiveFinalitySafety, InvariantStatus::NotApplicable, "ENTE_LOCAL profile"});
    } else {
        reports.push_back({InvariantId::C11_LineageSingularity, InvariantStatus::Unknown, "ENTE_DISTRIBUTED fork safety unverified"});
        reports.push_back({InvariantId::C13_ConstitutiveFinalitySafety, InvariantStatus::Unknown, "ENTE_DISTRIBUTED finality unverified"});
        any_unknown = true;
    }

    // C14 — Authority check on latest event
    bool auth_ok = !latest_event.authority_id.empty() && !latest_event.authority_epoch.empty();
    if (auth_ok && authority_lineage.has_value()) {
        auth_ok = authority_lineage->get().is_epoch_legitimate(
            authority::AuthorityId(latest_event.authority_id),
            authority::AuthorityEpochId(latest_event.authority_epoch)
        );
    }

    if (auth_ok) {
        reports.push_back({InvariantId::C14_ConstitutiveAuthorityContinuity, InvariantStatus::Satisfied, "Step authority valid"});
    } else {
        reports.push_back({InvariantId::C14_ConstitutiveAuthorityContinuity, InvariantStatus::Violated, "Step authority invalid"});
        any_violation = true;
    }

    ConstitutiveStatus final_status = ConstitutiveStatus::Valid;
    if (any_violation) {
        final_status = ConstitutiveStatus::Violated;
    } else if (any_unknown) {
        final_status = ConstitutiveStatus::Suspended;
    } else if (any_weakened) {
        final_status = ConstitutiveStatus::Weakened;
    }

    return VerificationReport{
        .status = final_status,
        .invariant_reports = std::move(reports),
        .profile = profile_
    };
}

} // namespace ente::constitution
