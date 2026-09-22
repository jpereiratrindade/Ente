#include "ente/judgment/fixture.hpp"
#include "ente/core/hash.hpp"
#include <format>

namespace ente::judgment {

void FixtureJudgmentEngine::set_canned_response(
    std::string_view interpretation_subject,
    std::string_view observation_subject,
    CompatibilityResult result,
    std::string rationale
) {
    std::string key = std::format("{}:{}", interpretation_subject, observation_subject);
    rules_[key] = Rule{
        .result = result,
        .rationale = std::move(rationale)
    };
}

JudgmentReport FixtureJudgmentEngine::evaluate_compatibility(
    const epistemic::Interpretation& current,
    const std::vector<epistemic::Observation>& new_evidence
) const noexcept {
    if (new_evidence.empty()) {
        return JudgmentReport{
            .compatibility = CompatibilityResult::Supported,
            .rationale = "No new challenging evidence",
            .engine_digest = core::HashUtil::sha256("fixture-default")
        };
    }

    // Check for contradictory evidence in the batch
    bool has_clear = false;
    bool has_blocked = false;
    for (const auto& obs : new_evidence) {
        if (obs.status == epistemic::EpistemicStatus::Contradictory) {
            return JudgmentReport{
                .compatibility = CompatibilityResult::Contradictory,
                .rationale = "Direct contradictory epistemic observation",
                .engine_digest = core::HashUtil::sha256("fixture-contradiction")
            };
        }
        if (obs.value == "true" && obs.subject == "path_clear") has_clear = true;
        if (obs.value == "false" && obs.subject == "path_clear") has_blocked = true;
    }

    if (has_clear && has_blocked) {
        return JudgmentReport{
            .compatibility = CompatibilityResult::Contradictory,
            .rationale = "Mutually contradictory observations present in same cycle",
            .engine_digest = core::HashUtil::sha256("fixture-contradiction")
        };
    }

    // Check matching rules
    for (const auto& obs : new_evidence) {
        std::string key = std::format("{}:{}", current.subject, obs.subject);
        auto it = rules_.find(key);
        if (it != rules_.end()) {
            return JudgmentReport{
                .compatibility = it->second.result,
                .rationale = it->second.rationale,
                .engine_digest = core::HashUtil::sha256(std::format("fixture:{}", key))
            };
        }

        // Automatic weakening if observation has unknown class or unexpected motion
        if (obs.subject == "unexpected_motion" || obs.status == epistemic::EpistemicStatus::Unknown) {
            return JudgmentReport{
                .compatibility = CompatibilityResult::Weakened,
                .rationale = "Unexpected motion or unclassified anomaly challenges path clear",
                .engine_digest = core::HashUtil::sha256("fixture-weakened-auto")
            };
        }
    }

    // Default to supported if nothing challenged it
    return JudgmentReport{
        .compatibility = CompatibilityResult::Supported,
        .rationale = "Evidence compatible with current interpretation",
        .engine_digest = core::HashUtil::sha256("fixture-compatible")
    };
}

} // namespace ente::judgment
