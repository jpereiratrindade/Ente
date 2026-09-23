#include "ente/judgment/status_engine.hpp"
#include "ente/core/hash.hpp"

#include <unordered_map>

namespace ente::judgment {

JudgmentReport StatusJudgmentEngine::evaluate_compatibility(
    const epistemic::Interpretation& current,
    const std::vector<epistemic::Observation>& new_evidence
) const noexcept {
    if (new_evidence.empty()) {
        return {
            .compatibility = CompatibilityResult::InsufficientEvidence,
            .rationale = "No new evidence was supplied",
            .engine_digest = core::HashUtil::sha256("status-engine:v1:insufficient")
        };
    }

    std::unordered_map<std::string, std::string> observed_values;
    bool uncertain = false;
    for (const auto& observation : new_evidence) {
        if (observation.status == epistemic::EpistemicStatus::Contradictory) {
            return {
                .compatibility = CompatibilityResult::Contradictory,
                .rationale = "Evidence is explicitly contradictory",
                .engine_digest = core::HashUtil::sha256("status-engine:v1:contradictory")
            };
        }
        if (observation.status == epistemic::EpistemicStatus::Unknown ||
            observation.status == epistemic::EpistemicStatus::Uncertain) {
            uncertain = true;
        }
        auto [it, inserted] = observed_values.emplace(observation.subject, observation.value);
        if (!inserted && it->second != observation.value) {
            return {
                .compatibility = CompatibilityResult::Contradictory,
                .rationale = "Same-subject observations carry conflicting values",
                .engine_digest = core::HashUtil::sha256("status-engine:v1:value-conflict")
            };
        }
    }

    if (uncertain) {
        return {
            .compatibility = CompatibilityResult::Weakened,
            .rationale = "Evidence contains explicit uncertainty",
            .engine_digest = core::HashUtil::sha256("status-engine:v1:weakened")
        };
    }

    const std::string material = "status-engine:v1:supported:" + current.subject;
    return {
        .compatibility = CompatibilityResult::Supported,
        .rationale = "Typed evidence contains no contradiction or uncertainty",
        .engine_digest = core::HashUtil::sha256(material)
    };
}

} // namespace ente::judgment
