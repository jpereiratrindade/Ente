#pragma once

#include "ente/judgment/engine.hpp"
#include <map>
#include <functional>

namespace ente::judgment {

class FixtureJudgmentEngine final : public JudgmentEngine {
public:
    FixtureJudgmentEngine() = default;

    void set_canned_response(
        std::string_view interpretation_subject,
        std::string_view observation_subject,
        CompatibilityResult result,
        std::string rationale
    );

    [[nodiscard]] JudgmentReport evaluate_compatibility(
        const epistemic::Interpretation& current,
        const std::vector<epistemic::Observation>& new_evidence
    ) const noexcept override;

private:
    struct Rule {
        CompatibilityResult result;
        std::string rationale;
    };

    std::map<std::string, Rule> rules_;
};

} // namespace ente::judgment
