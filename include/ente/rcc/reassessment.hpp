#pragma once

#include "ente/core/types.hpp"
#include "ente/epistemic/interpretation.hpp"
#include "ente/epistemic/observation.hpp"
#include "ente/judgment/engine.hpp"
#include "ente/rcc/actions.hpp"
#include <vector>
#include <optional>

namespace ente::rcc {

enum class RCCState : uint8_t {
    Stable,
    Weakened,
    Reassessing,
    EvidenceSeeking,
    Suspended,
    Reinterpreting,
    CoherenceRestored,
    Unresolved
};

[[nodiscard]] constexpr std::string_view to_string(RCCState s) noexcept {
    switch (s) {
        case RCCState::Stable: return "STABLE";
        case RCCState::Weakened: return "WEAKENED";
        case RCCState::Reassessing: return "REASSESSING";
        case RCCState::EvidenceSeeking: return "EVIDENCE_SEEKING";
        case RCCState::Suspended: return "SUSPENDED";
        case RCCState::Reinterpreting: return "REINTERPRETING";
        case RCCState::CoherenceRestored: return "COHERENCE_RESTORED";
        case RCCState::Unresolved: return "UNRESOLVED";
    }
    return "UNKNOWN_RCC_STATE";
}

struct ReassessmentResult {
    judgment::CompatibilityResult compatibility;
    EpistemicAction epistemic_action;
    RCCState state_after;
    std::vector<core::EvidenceId> challenging_evidence;
    std::string reason;
};

class ContextReassessment {
public:
    ContextReassessment() = default;

    [[nodiscard]] ReassessmentResult evaluate(
        epistemic::Interpretation& current_interpretation,
        const std::vector<epistemic::Observation>& new_evidence,
        const judgment::JudgmentEngine& engine
    ) noexcept;

    [[nodiscard]] RCCState current_state() const noexcept { return state_; }

private:
    RCCState state_{RCCState::Stable};
};

} // namespace ente::rcc
