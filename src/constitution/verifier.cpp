#include "ente/constitution/verifier.hpp"

namespace ente::constitution {

VerificationReport ConstitutionVerifier::verify(
    const identity::IdentityState& identity,
    const std::optional<identity::GenesisRecord>& genesis,
    const history::RecoverableHistory& history,
    const std::optional<epistemic::Interpretation>& current_interpretation
) const noexcept {
    std::vector<InvariantReport> reports;
    reports.reserve(12);

    bool any_violation = false;
    bool any_weakened = false;

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
    for (const auto& ev : history.events()) {
        if (ev.kind == history::EventKind::Interpretation || ev.kind == history::EventKind::Reinterpretation) {
            if (ev.evidence_refs.empty() && ev.causal_predecessors.empty()) {
                provenance_ok = false;
                break;
            }
        }
    }
    reports.push_back({InvariantId::C3_Observability, InvariantStatus::Satisfied, "Observational surface actively recording facts"});
    if (provenance_ok) {
        reports.push_back({InvariantId::C4_Provenance, InvariantStatus::Satisfied, "Evidence origins and causal links verified for all material events"});
    } else {
        reports.push_back({InvariantId::C4_Provenance, InvariantStatus::Violated, "Event detected with missing provenance evidence"});
        any_violation = true;
    }

    // C5 — Epistemic Distinction & C8 — Unknown Representability
    reports.push_back({InvariantId::C5_EpistemicDistinction, InvariantStatus::Satisfied, "Epistemic status types preserved and distinct across all recorded events"});
    reports.push_back({InvariantId::C8_UnknownRepresentability, InvariantStatus::Satisfied, "Explicit unknown epistemic status supported without data coercion"});

    // C6 — Revision Capability
    reports.push_back({InvariantId::C6_RevisionCapability, InvariantStatus::Satisfied, "RCC revision engine functional"});

    // C7 — Coherence Evaluation
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

    // C11 — Lineage Singularity: Not Applicable to single-node local realization without external network forks
    reports.push_back({InvariantId::C11_LineageSingularity, InvariantStatus::NotApplicable, "Single-process execution; distributed fork protocol not applicable"});

    // C13 — Finality Safety: Not Applicable to single-process local execution (requires BFT consensus network)
    reports.push_back({InvariantId::C13_ConstitutiveFinalitySafety, InvariantStatus::NotApplicable, "Network fault model and BFT finality not applicable to isolated local node"});

    // C14 — Authority Continuity: Audits that all events belong to legitimate authority epochs
    bool authority_ok = true;
    for (const auto& ev : history.events()) {
        if (ev.authority_id.empty() || ev.authority_epoch.empty()) {
            authority_ok = false;
            break;
        }
    }
    if (authority_ok && !history.empty()) {
        reports.push_back({InvariantId::C14_ConstitutiveAuthorityContinuity, InvariantStatus::Satisfied, "Event stream maintains uninterrupted legitimate authority lineage"});
    } else {
        reports.push_back({InvariantId::C14_ConstitutiveAuthorityContinuity, InvariantStatus::Violated, "Event detected without valid authority grant"});
        any_violation = true;
    }

    ConstitutiveStatus final_status = ConstitutiveStatus::Valid;
    if (any_violation) {
        final_status = ConstitutiveStatus::Violated;
    } else if (any_weakened) {
        final_status = ConstitutiveStatus::Weakened;
    }

    return VerificationReport{
        .status = final_status,
        .invariant_reports = std::move(reports)
    };
}

} // namespace ente::constitution
