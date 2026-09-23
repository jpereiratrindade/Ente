#include "ente/judgment/fixture.hpp"
#include "ente/core/hash.hpp"
#include <format>

#include <unordered_map>

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

    // 1. Generic Contradiction Detection (direct Contradictory status or boolean/semantic polar opposites)
    auto is_polar_opposite = [](std::string_view a, std::string_view b) noexcept -> bool {
        if ((a == "true" && b == "false") || (a == "false" && b == "true")) return true;
        if ((a == "dry" && b == "wet") || (a == "wet" && b == "dry")) return true;
        if ((a == "clear" && b == "blocked") || (a == "blocked" && b == "clear")) return true;
        return false;
    };

    std::unordered_map<std::string, std::string> seen_subject_values;
    for (const auto& obs : new_evidence) {
        if (obs.status == epistemic::EpistemicStatus::Contradictory) {
            return JudgmentReport{
                .compatibility = CompatibilityResult::Contradictory,
                .rationale = std::format("Direct contradictory epistemic observation on '{}'", obs.subject),
                .engine_digest = core::HashUtil::sha256("fixture-contradiction")
            };
        }
        auto it = seen_subject_values.find(obs.subject);
        if (it != seen_subject_values.end() && is_polar_opposite(it->second, obs.value)) {
            return JudgmentReport{
                .compatibility = CompatibilityResult::Contradictory,
                .rationale = std::format("Mutually contradictory polar observations for subject '{}' in same cycle ('{}' vs '{}')",
                    obs.subject, it->second, obs.value),
                .engine_digest = core::HashUtil::sha256("fixture-contradiction")
            };
        }
        seen_subject_values[obs.subject] = obs.value;
    }

    // 2. Custom Explicit Domain Rules
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

        // 3. Generic Epistemic Weakening for Unknown or Uncertain observations
        if (obs.status == epistemic::EpistemicStatus::Unknown ||
            obs.status == epistemic::EpistemicStatus::Uncertain ||
            obs.subject.find("anomaly") != std::string::npos) {
            return JudgmentReport{
                .compatibility = CompatibilityResult::Weakened,
                .rationale = std::format("Epistemic uncertainty or unknown anomaly in subject '{}'", obs.subject),
                .engine_digest = core::HashUtil::sha256("fixture-weakened-auto")
            };
        }
    }

    // Default to supported if no rules or anomalies challenged it
    return JudgmentReport{
        .compatibility = CompatibilityResult::Supported,
        .rationale = "Evidence compatible with current interpretation",
        .engine_digest = core::HashUtil::sha256("fixture-compatible")
    };
}

} // namespace ente::judgment
