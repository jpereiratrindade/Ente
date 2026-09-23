#include "ente/constitution/verifier.hpp"
#include "ente/history/payloads.hpp"

#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace ente::constitution {

namespace {

bool known_compatibility(std::string_view value) noexcept {
    return value == "SUPPORTED" || value == "WEAKENED" ||
        value == "CONTRADICTORY" || value == "INSUFFICIENT_EVIDENCE" ||
        value == "UNKNOWN";
}

std::optional<epistemic::InterpretationStatus> status_for_compatibility(
    std::string_view value
) noexcept {
    if (value == "SUPPORTED") return epistemic::InterpretationStatus::Supported;
    if (value == "WEAKENED") return epistemic::InterpretationStatus::Weakened;
    if (value == "CONTRADICTORY") return epistemic::InterpretationStatus::Contradicted;
    if (value == "INSUFFICIENT_EVIDENCE" || value == "UNKNOWN") {
        return epistemic::InterpretationStatus::Unknown;
    }
    return std::nullopt;
}

bool known_constitutive_status(std::string_view value) noexcept {
    return value == "VALID" || value == "WEAKENED" ||
        value == "SUSPENDED" || value == "VIOLATED";
}

std::unordered_set<std::string> evidence_set(
    const std::vector<core::EvidenceId>& evidence
) {
    std::unordered_set<std::string> result;
    for (const auto& id : evidence) result.insert(id.value);
    return result;
}

bool interpretation_evidence_matches(
    const history::InterpretationPayload& payload,
    const history::HistoryEvent& event,
    const std::unordered_set<std::string>& known_evidence
) {
    if (payload.supporting_evidence.empty()) return false;
    std::unordered_set<std::string> declared;
    for (const auto& id : payload.supporting_evidence) {
        if (!known_evidence.contains(id.value) || !declared.insert(id.value).second) return false;
    }
    for (const auto& id : payload.challenging_evidence) {
        if (!known_evidence.contains(id.value) || !declared.insert(id.value).second) return false;
    }
    return declared == evidence_set(event.evidence_refs);
}

void add_report(
    std::vector<InvariantReport>& reports,
    InvariantId id,
    InvariantStatus status,
    std::string details,
    std::vector<core::EventId> support = {}
) {
    reports.push_back({id, status, std::move(details), std::move(support)});
}

struct BiographyAudit {
    bool has_observation{false};
    bool requires_observation{false};
    bool provenance_ok{true};
    bool epistemic_ok{true};
    bool revision_ok{true};
    bool coherence_ok{true};
    std::vector<core::EventId> provenance_support;
    std::vector<core::EventId> revision_support;
    std::vector<core::EventId> coherence_support;
    std::optional<std::string> last_compatibility;
    size_t last_judgment_index{0};
    size_t last_interpretation_index{0};
};

BiographyAudit audit_biography(const history::RecoverableHistory& history) {
    BiographyAudit audit;
    std::unordered_set<std::string> known_evidence;
    std::unordered_map<std::string, size_t> interpretations;
    std::optional<history::JudgmentPayload> latest_judgment;
    std::optional<history::PerturbationPayload> pending_perturbation;

    for (size_t index = 0; index < history.events().size(); ++index) {
        const auto& event = history.events()[index];

        if (event.kind != history::EventKind::Genesis &&
            event.kind != history::EventKind::Adaptation &&
            event.kind != history::EventKind::AuthorityTransition) {
            audit.requires_observation = true;
        }

        if (event.kind == history::EventKind::Observation) {
            const auto payload = history::parse_observation_payload(event.payload_content);
            if (!payload.has_value() || event.evidence_refs.size() != 1 ||
                event.evidence_refs.front() != payload->evidence_id ||
                payload->observed_at > event.logical_time ||
                !known_evidence.insert(payload->evidence_id.value).second) {
                audit.provenance_ok = false;
                audit.epistemic_ok = false;
            } else {
                audit.has_observation = true;
                audit.provenance_support.push_back(event.id);
            }
            continue;
        }

        for (const auto& evidence : event.evidence_refs) {
            if (!known_evidence.contains(evidence.value)) audit.provenance_ok = false;
        }

        if (event.kind == history::EventKind::Interpretation ||
            event.kind == history::EventKind::Reinterpretation ||
            event.kind == history::EventKind::CoherenceRestored) {
            const auto payload = history::parse_interpretation_payload(event.payload_content);
            if (!payload.has_value() || payload->created_at != event.logical_time ||
                !interpretation_evidence_matches(*payload, event, known_evidence) ||
                interpretations.contains(payload->id.value)) {
                audit.provenance_ok = false;
                audit.revision_ok = false;
                continue;
            }
            if (event.kind == history::EventKind::Interpretation) {
                if (payload->supersedes.has_value()) audit.revision_ok = false;
            } else if (!payload->supersedes.has_value() ||
                       !interpretations.contains(payload->supersedes->value) ||
                       event.causal_predecessors.empty()) {
                audit.revision_ok = false;
            } else {
                audit.revision_support.push_back(event.id);
            }
            interpretations.emplace(payload->id.value, index);
            audit.last_interpretation_index = index;
        }

        if (event.kind == history::EventKind::Judgment) {
            const auto payload = history::parse_judgment_payload(event.payload_content);
            if (!payload.has_value() || !known_compatibility(payload->compatibility) ||
                payload->engine_digest.is_zero()) {
                audit.coherence_ok = false;
            } else {
                latest_judgment = *payload;
                audit.last_compatibility = payload->compatibility;
                audit.last_judgment_index = index;
                audit.coherence_support.push_back(event.id);
            }
        }

        if (event.kind == history::EventKind::Perturbation) {
            const auto payload = history::parse_perturbation_payload(event.payload_content);
            if (!payload.has_value() || !latest_judgment.has_value() ||
                latest_judgment->compatibility != payload->compatibility ||
                payload->compatibility == "SUPPORTED" ||
                payload->recommended_action == rcc::EpistemicAction::Keep) {
                audit.revision_ok = false;
                audit.coherence_ok = false;
            } else {
                pending_perturbation = *payload;
                audit.revision_support.push_back(event.id);
            }
        }

        if (event.kind == history::EventKind::EpistemicAction) {
            const auto payload = history::parse_epistemic_action_payload(event.payload_content);
            if (!payload.has_value() || !pending_perturbation.has_value() ||
                payload->action != pending_perturbation->recommended_action) {
                audit.revision_ok = false;
            } else {
                pending_perturbation.reset();
                audit.revision_support.push_back(event.id);
            }
        }

        if (event.kind == history::EventKind::ActionAuthorized) {
            const auto assurance = history::parse_assurance_decision_payload(event.payload_content);
            if (assurance.has_value() &&
                !known_constitutive_status(assurance->constitutive_status)) {
                audit.coherence_ok = false;
            }
        }

        if (event.kind == history::EventKind::EffectObservation) {
            const auto action = history::parse_action_transaction(event.payload_content);
            if (!action.has_value() ||
                (action->phase == history::ActionPhase::Confirmed &&
                 event.evidence_refs.empty())) {
                audit.provenance_ok = false;
            }
        }
    }

    if (pending_perturbation.has_value()) audit.revision_ok = false;
    return audit;
}

} // namespace

VerificationReport ConstitutionVerifier::verify(
    const identity::IdentityState& identity,
    const std::optional<identity::GenesisRecord>& genesis,
    const history::RecoverableHistory& history,
    const std::optional<epistemic::Interpretation>& current_interpretation,
    std::optional<std::reference_wrapper<const authority::AuthorityLineage>> authority_lineage
) const noexcept {
    std::vector<InvariantReport> reports;
    reports.reserve(14);
    bool violation = false;
    bool weakened = false;
    bool unknown = false;

    const bool identity_ok = !identity.id.empty() &&
        identity.lifecycle == identity::LifecycleStatus::LifeActive;
    add_report(reports, InvariantId::C1_Identity,
        identity_ok ? InvariantStatus::Satisfied : InvariantStatus::Violated,
        identity_ok ? "Active singular identity established" : "Identity missing or inactive");
    violation |= !identity_ok;

    bool genesis_ok = false;
    if (genesis.has_value() && !history.empty()) {
        const auto payload = history::parse_genesis_payload(history.events().front().payload_content);
        genesis_ok = payload.has_value() && genesis->identity == identity.id &&
            payload->identity == identity.id &&
            payload->genesis_digest == genesis->genesis_digest &&
            !genesis->genesis_digest.is_zero();
    }
    add_report(reports, InvariantId::C9_GenesisAnchor,
        genesis_ok ? InvariantStatus::Satisfied : InvariantStatus::Violated,
        genesis_ok ? "Genesis payload, record and identity agree" : "Genesis anchor mismatch");
    violation |= !genesis_ok;

    const bool history_ok = history.verify_integrity() && !history.empty();
    add_report(reports, InvariantId::C10_TemporalIntegrity,
        history_ok ? InvariantStatus::Satisfied : InvariantStatus::Violated,
        history_ok ? "Canonical hash-chain, ordering and causal references verified" :
                     "Temporal or canonical history integrity failed");
    add_report(reports, InvariantId::C12_HistoryRecoverability,
        history_ok ? InvariantStatus::Satisfied : InvariantStatus::Violated,
        history_ok ? "Every event has a recognized recoverable schema" :
                     "History cannot be deterministically reconstructed");
    violation |= !history_ok;

    bool continuity_ok = history_ok;
    if (continuity_ok) {
        for (const auto& event : history.events()) {
            if (event.identity != identity.id) {
                continuity_ok = false;
                break;
            }
        }
    }
    add_report(reports, InvariantId::C2_Continuity,
        continuity_ok ? InvariantStatus::Satisfied : InvariantStatus::Violated,
        continuity_ok ? "Every event is bound to the Genesis identity" :
                        "Identity discontinuity detected");
    violation |= !continuity_ok;

    const auto biography = audit_biography(history);
    const bool observability_ok = !biography.requires_observation || biography.has_observation;
    add_report(reports, InvariantId::C3_Observability,
        observability_ok ? InvariantStatus::Satisfied : InvariantStatus::Violated,
        observability_ok ? "Typed observations establish the factual surface" :
                           "Active biography contains no valid observation",
        biography.provenance_support);
    violation |= !observability_ok;

    add_report(reports, InvariantId::C4_Provenance,
        biography.provenance_ok ? InvariantStatus::Satisfied : InvariantStatus::Violated,
        biography.provenance_ok ? "All evidence references resolve to prior typed observations" :
                                  "Missing, duplicated or inconsistent provenance",
        biography.provenance_support);
    violation |= !biography.provenance_ok;

    add_report(reports, InvariantId::C5_EpistemicDistinction,
        biography.epistemic_ok ? InvariantStatus::Satisfied : InvariantStatus::Violated,
        biography.epistemic_ok ? "Every observation carries a valid epistemic status" :
                                 "Invalid epistemic observation encountered");
    add_report(reports, InvariantId::C8_UnknownRepresentability,
        biography.epistemic_ok ? InvariantStatus::Satisfied : InvariantStatus::Violated,
        biography.epistemic_ok ? "Unknown and uncertainty remain explicit typed states" :
                                 "Epistemic state was not representable without coercion");
    violation |= !biography.epistemic_ok;

    add_report(reports, InvariantId::C6_RevisionCapability,
        biography.revision_ok ? InvariantStatus::Satisfied : InvariantStatus::Violated,
        biography.revision_ok ?
            "Every claimed perturbation, epistemic action and revision forms a traceable sequence" :
            "Broken perturbation or revision sequence",
        biography.revision_support);
    violation |= !biography.revision_ok;

    bool coherence_ok = biography.coherence_ok;
    if (coherence_ok && biography.last_compatibility.has_value() &&
        biography.last_judgment_index >= biography.last_interpretation_index) {
        const auto expected = status_for_compatibility(*biography.last_compatibility);
        coherence_ok = expected.has_value() && current_interpretation.has_value() &&
            current_interpretation->status == *expected;
    }
    add_report(reports, InvariantId::C7_CoherenceEvaluation,
        coherence_ok ? InvariantStatus::Satisfied : InvariantStatus::Violated,
        coherence_ok ? "Judgments and active interpretation agree with recorded evidence" :
                       "Judgment/interpretation coherence mismatch",
        biography.coherence_support);
    violation |= !coherence_ok;
    if (coherence_ok && current_interpretation.has_value() &&
        (current_interpretation->status == epistemic::InterpretationStatus::Weakened ||
         current_interpretation->status == epistemic::InterpretationStatus::Contradicted ||
         current_interpretation->status == epistemic::InterpretationStatus::Unknown)) {
        weakened = true;
    }

    if (profile_ == ConformanceProfile::EnteLocal) {
        add_report(reports, InvariantId::C11_LineageSingularity,
            InvariantStatus::NotApplicable, "ENTE_LOCAL is single-node");
        add_report(reports, InvariantId::C13_ConstitutiveFinalitySafety,
            InvariantStatus::NotApplicable, "ENTE_LOCAL does not claim distributed finality");
    } else {
        add_report(reports, InvariantId::C11_LineageSingularity,
            InvariantStatus::Unknown, "Distributed fork safety lacks external evidence");
        add_report(reports, InvariantId::C13_ConstitutiveFinalitySafety,
            InvariantStatus::Unknown, "Distributed finality lacks external evidence");
        unknown = true;
    }

    bool authority_ok = authority_lineage.has_value() &&
        authority_lineage->get().verify_lineage_integrity();
    std::vector<core::EventId> authority_support;
    if (authority_ok) {
        for (const auto& event : history.events()) {
            if (!authority_lineage->get().is_epoch_legitimate_at(
                    authority::AuthorityId(event.authority_id),
                    authority::AuthorityEpochId(event.authority_epoch),
                    event.logical_time)) {
                authority_ok = false;
                break;
            }
            if (event.kind == history::EventKind::AuthorityTransition) {
                authority_support.push_back(event.id);
            }
        }
    }
    add_report(reports, InvariantId::C14_ConstitutiveAuthorityContinuity,
        authority_ok ? InvariantStatus::Satisfied : InvariantStatus::Violated,
        authority_ok ? "Authenticated authority delegation and temporal windows verified" :
                       "Authority lineage missing, unsigned or temporally invalid",
        std::move(authority_support));
    violation |= !authority_ok;

    const auto status = violation ? ConstitutiveStatus::Violated :
        unknown ? ConstitutiveStatus::Suspended :
        weakened ? ConstitutiveStatus::Weakened : ConstitutiveStatus::Valid;
    return {.status = status, .invariant_reports = std::move(reports), .profile = profile_};
}

VerificationReport ConstitutionVerifier::verify_step(
    const identity::IdentityState& identity,
    const std::optional<identity::GenesisRecord>& genesis,
    const history::RecoverableHistory& history,
    const std::optional<epistemic::Interpretation>& current_interpretation,
    std::optional<std::reference_wrapper<const authority::AuthorityLineage>> authority_lineage
) const noexcept {
    std::vector<InvariantReport> reports;
    reports.reserve(14);
    bool violation = false;
    bool weakened = false;
    bool unknown = false;

    const bool has_tail = !history.empty();
    const auto* latest = has_tail ? &history.head() : nullptr;
    const bool identity_ok = latest != nullptr && !identity.id.empty() &&
        identity.lifecycle == identity::LifecycleStatus::LifeActive &&
        latest->identity == identity.id;
    add_report(reports, InvariantId::C1_Identity,
        identity_ok ? InvariantStatus::Satisfied : InvariantStatus::Violated,
        identity_ok ? "Tail bound to active identity" : "Tail identity invalid");
    add_report(reports, InvariantId::C2_Continuity,
        identity_ok ? InvariantStatus::Satisfied : InvariantStatus::Violated,
        identity_ok ? "Tail preserves Genesis identity" : "Tail identity discontinuity");
    violation |= !identity_ok;

    const bool genesis_ok = genesis.has_value() && genesis->identity == identity.id &&
        !genesis->genesis_digest.is_zero();
    add_report(reports, InvariantId::C9_GenesisAnchor,
        genesis_ok ? InvariantStatus::Satisfied : InvariantStatus::Violated,
        genesis_ok ? "Genesis record active" : "Genesis record invalid");
    violation |= !genesis_ok;

    const bool tail_ok = has_tail && history.verify_tail();
    add_report(reports, InvariantId::C10_TemporalIntegrity,
        tail_ok ? InvariantStatus::Satisfied : InvariantStatus::Violated,
        tail_ok ? "Canonical tail and previous digest verified" : "Tail integrity failed");
    add_report(reports, InvariantId::C12_HistoryRecoverability,
        tail_ok ? InvariantStatus::Satisfied : InvariantStatus::Violated,
        tail_ok ? "Tail payload has a recoverable typed schema" : "Tail is not recoverable");
    violation |= !tail_ok;

    bool provenance_ok = tail_ok;
    bool epistemic_ok = tail_ok;
    bool revision_ok = tail_ok;
    bool coherence_ok = tail_ok;
    if (latest != nullptr) {
        for (const auto& evidence : latest->evidence_refs) {
            provenance_ok &= history.contains_evidence(evidence);
        }
        if (latest->kind == history::EventKind::Observation) {
            const auto observation = history::parse_observation_payload(latest->payload_content);
            epistemic_ok = observation.has_value() && latest->evidence_refs.size() == 1 &&
                latest->evidence_refs.front() == observation->evidence_id &&
                observation->observed_at <= latest->logical_time;
        }
        if (latest->kind == history::EventKind::Reinterpretation) {
            const auto interpretation = history::parse_interpretation_payload(latest->payload_content);
            revision_ok = interpretation.has_value() && interpretation->supersedes.has_value() &&
                !latest->causal_predecessors.empty() && !latest->evidence_refs.empty();
        }
        if (latest->kind == history::EventKind::Judgment) {
            const auto judgment = history::parse_judgment_payload(latest->payload_content);
            const auto expected = judgment.has_value()
                ? status_for_compatibility(judgment->compatibility)
                : std::nullopt;
            coherence_ok = judgment.has_value() && expected.has_value() &&
                !judgment->engine_digest.is_zero() && current_interpretation.has_value() &&
                current_interpretation->status == *expected;
        }
    }

    add_report(reports, InvariantId::C3_Observability,
        epistemic_ok ? InvariantStatus::Satisfied : InvariantStatus::Violated,
        epistemic_ok ? "Latest observational claim is typed or none is claimed" :
                       "Latest observation is malformed");
    add_report(reports, InvariantId::C4_Provenance,
        provenance_ok ? InvariantStatus::Satisfied : InvariantStatus::Violated,
        provenance_ok ? "Tail evidence resolves in REC" : "Tail evidence provenance failed");
    add_report(reports, InvariantId::C5_EpistemicDistinction,
        epistemic_ok ? InvariantStatus::Satisfied : InvariantStatus::Violated,
        epistemic_ok ? "Tail epistemic type is valid" : "Tail epistemic type invalid");
    add_report(reports, InvariantId::C8_UnknownRepresentability,
        epistemic_ok ? InvariantStatus::Satisfied : InvariantStatus::Violated,
        epistemic_ok ? "Typed uncertainty preserved" : "Uncertainty representation invalid");
    add_report(reports, InvariantId::C6_RevisionCapability,
        revision_ok ? InvariantStatus::Satisfied : InvariantStatus::Violated,
        revision_ok ? "Any tail revision is causally traceable" :
                      "Tail revision lacks predecessor or evidence");
    add_report(reports, InvariantId::C7_CoherenceEvaluation,
        coherence_ok ? InvariantStatus::Satisfied : InvariantStatus::Violated,
        coherence_ok ? "Latest judgment agrees with active interpretation" :
                       "Latest judgment contradicts active interpretation state");
    violation |= !provenance_ok || !epistemic_ok || !revision_ok || !coherence_ok;

    if (current_interpretation.has_value() &&
        (current_interpretation->status == epistemic::InterpretationStatus::Weakened ||
         current_interpretation->status == epistemic::InterpretationStatus::Contradicted ||
         current_interpretation->status == epistemic::InterpretationStatus::Unknown)) {
        weakened = true;
    }

    if (profile_ == ConformanceProfile::EnteLocal) {
        add_report(reports, InvariantId::C11_LineageSingularity,
            InvariantStatus::NotApplicable, "ENTE_LOCAL is single-node");
        add_report(reports, InvariantId::C13_ConstitutiveFinalitySafety,
            InvariantStatus::NotApplicable, "ENTE_LOCAL has no distributed finality claim");
    } else {
        add_report(reports, InvariantId::C11_LineageSingularity,
            InvariantStatus::Unknown, "Distributed fork evidence unavailable");
        add_report(reports, InvariantId::C13_ConstitutiveFinalitySafety,
            InvariantStatus::Unknown, "Distributed finality evidence unavailable");
        unknown = true;
    }

    const bool authority_ok = latest != nullptr && authority_lineage.has_value() &&
        authority_lineage->get().verify_lineage_integrity() &&
        authority_lineage->get().is_epoch_legitimate_at(
            authority::AuthorityId(latest->authority_id),
            authority::AuthorityEpochId(latest->authority_epoch),
            latest->logical_time);
    add_report(reports, InvariantId::C14_ConstitutiveAuthorityContinuity,
        authority_ok ? InvariantStatus::Satisfied : InvariantStatus::Violated,
        authority_ok ? "Tail authority has an authenticated valid epoch" :
                       "Tail authority is not authenticated for this time");
    violation |= !authority_ok;

    const auto status = violation ? ConstitutiveStatus::Violated :
        unknown ? ConstitutiveStatus::Suspended :
        weakened ? ConstitutiveStatus::Weakened : ConstitutiveStatus::Valid;
    return {.status = status, .invariant_reports = std::move(reports), .profile = profile_};
}

} // namespace ente::constitution
