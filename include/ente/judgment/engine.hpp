#pragma once

#include "ente/core/types.hpp"
#include "ente/epistemic/interpretation.hpp"
#include "ente/epistemic/observation.hpp"
#include <string>
#include <vector>

namespace ente::judgment {

enum class CompatibilityResult : uint8_t {
    Supported,
    Weakened,
    Contradictory,
    InsufficientEvidence,
    Unknown
};

[[nodiscard]] constexpr std::string_view to_string(CompatibilityResult r) noexcept {
    switch (r) {
        case CompatibilityResult::Supported: return "SUPPORTED";
        case CompatibilityResult::Weakened: return "WEAKENED";
        case CompatibilityResult::Contradictory: return "CONTRADICTORY";
        case CompatibilityResult::InsufficientEvidence: return "INSUFFICIENT_EVIDENCE";
        case CompatibilityResult::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN_COMPATIBILITY";
}

struct JudgmentReport {
    CompatibilityResult compatibility;
    std::string rationale;
    core::Digest engine_digest;
};

class JudgmentEngine {
public:
    virtual ~JudgmentEngine() = default;

    [[nodiscard]] virtual JudgmentReport evaluate_compatibility(
        const epistemic::Interpretation& current,
        const std::vector<epistemic::Observation>& new_evidence
    ) const noexcept = 0;
};

} // namespace ente::judgment
