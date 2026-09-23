#include "ente/rcc/reassessment.hpp"

namespace ente::rcc {

ReassessmentResult ContextReassessment::evaluate(
    epistemic::Interpretation& current_interpretation,
    const std::vector<epistemic::Observation>& new_evidence,
    const judgment::JudgmentEngine& engine
) noexcept {
    auto judgment = engine.evaluate_compatibility(current_interpretation, new_evidence);

    ReassessmentResult res{
        .compatibility = judgment.compatibility,
        .epistemic_action = EpistemicAction::Keep,
        .state_after = RCCState::Stable,
        .challenging_evidence = {},
        .reason = judgment.rationale,
        .judgment_engine_digest = judgment.engine_digest,
        .evidence_request = std::nullopt
    };

    switch (judgment.compatibility) {
        case judgment::CompatibilityResult::Supported:
            current_interpretation.status = epistemic::InterpretationStatus::Supported;
            state_ = RCCState::Stable;
            res.epistemic_action = EpistemicAction::Keep;
            res.state_after = RCCState::Stable;
            break;

        case judgment::CompatibilityResult::Weakened:
            current_interpretation.status = epistemic::InterpretationStatus::Weakened;
            for (const auto& obs : new_evidence) {
                current_interpretation.challenging_evidence.push_back(obs.id);
                res.challenging_evidence.push_back(obs.id);
            }
            state_ = RCCState::Suspended;
            res.epistemic_action = EpistemicAction::SuspendAction;
            res.state_after = RCCState::Suspended;
            break;

        case judgment::CompatibilityResult::Contradictory: {
            current_interpretation.status = epistemic::InterpretationStatus::Contradicted;
            std::vector<std::string> conflict_sources;
            for (const auto& obs : new_evidence) {
                current_interpretation.challenging_evidence.push_back(obs.id);
                res.challenging_evidence.push_back(obs.id);
                conflict_sources.push_back(obs.subject);
            }
            state_ = RCCState::EvidenceSeeking;
            res.epistemic_action = EpistemicAction::SeekEvidence;
            res.state_after = RCCState::EvidenceSeeking;
            res.evidence_request = epistemic::EvidenceRequest{
                .subject = current_interpretation.subject,
                .purpose = epistemic::EvidenceDemandPurpose::DiscriminateContradiction,
                .conflicting_sources = std::move(conflict_sources),
                .suggested_discriminator = "discriminating_reference_probe_or_impedance_test",
                .demand_summary = "Contradiction detected in active observations. Discriminating evidence required."
            };
            break;
        }

        case judgment::CompatibilityResult::InsufficientEvidence:
        case judgment::CompatibilityResult::Unknown: {
            current_interpretation.status = epistemic::InterpretationStatus::Unknown;
            std::vector<std::string> sources;
            for (const auto& obs : new_evidence) {
                current_interpretation.challenging_evidence.push_back(obs.id);
                res.challenging_evidence.push_back(obs.id);
                sources.push_back(obs.subject);
            }
            state_ = RCCState::Reassessing;
            res.epistemic_action = EpistemicAction::SeekEvidence;
            res.state_after = RCCState::Reassessing;
            res.evidence_request = epistemic::EvidenceRequest{
                .subject = current_interpretation.subject,
                .purpose = epistemic::EvidenceDemandPurpose::ResolveUnknown,
                .conflicting_sources = std::move(sources),
                .suggested_discriminator = "telemetry_refresh_or_diagnostic_probe",
                .demand_summary = "Epistemic state is Unknown/Insufficient. Resolving evidence required."
            };
            break;
        }
    }

    return res;
}

} // namespace ente::rcc
