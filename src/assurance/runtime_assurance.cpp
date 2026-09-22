#include "ente/assurance/runtime_assurance.hpp"

namespace ente::assurance {

SafetyDirective RuntimeAssurance::evaluate_safety(
    rcc::EpistemicAction epistemic_action,
    constitution::ConstitutiveStatus constitutive_status
) const noexcept {
    // 1. Constitutional Violations override any operational desire
    if (constitutive_status == constitution::ConstitutiveStatus::Violated) {
        return SafetyDirective::EmergencyStop;
    }

    if (constitutive_status == constitution::ConstitutiveStatus::Suspended) {
        return SafetyDirective::SafeHold;
    }

    // 2. Epistemic Actions determine operational degradation / hold
    switch (epistemic_action) {
        case rcc::EpistemicAction::Keep:
            return SafetyDirective::AllowAction;

        case rcc::EpistemicAction::Reobserve:
            return SafetyDirective::DegradePerformance;

        case rcc::EpistemicAction::SeekEvidence:
        case rcc::EpistemicAction::SuspendAction:
        case rcc::EpistemicAction::SuspendJudgment:
            return SafetyDirective::SafeHold;

        case rcc::EpistemicAction::Reinterpret:
            return SafetyDirective::SafeHold;
    }

    return SafetyDirective::SafeHold;
}

} // namespace ente::assurance
