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
        .reason = judgment.rationale
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

        case judgment::CompatibilityResult::Contradictory:
            current_interpretation.status = epistemic::InterpretationStatus::Contradicted;
            for (const auto& obs : new_evidence) {
                current_interpretation.challenging_evidence.push_back(obs.id);
                res.challenging_evidence.push_back(obs.id);
            }
            state_ = RCCState::EvidenceSeeking;
            res.epistemic_action = EpistemicAction::SeekEvidence;
            res.state_after = RCCState::EvidenceSeeking;
            break;

        case judgment::CompatibilityResult::InsufficientEvidence:
        case judgment::CompatibilityResult::Unknown:
            current_interpretation.status = epistemic::InterpretationStatus::Unknown;
            for (const auto& obs : new_evidence) {
                current_interpretation.challenging_evidence.push_back(obs.id);
                res.challenging_evidence.push_back(obs.id);
            }
            state_ = RCCState::Reassessing;
            res.epistemic_action = EpistemicAction::SeekEvidence;
            res.state_after = RCCState::Reassessing;
            break;
    }

    return res;
}

} // namespace ente::rcc
