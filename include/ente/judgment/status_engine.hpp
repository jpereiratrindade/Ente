#pragma once

#include "ente/judgment/engine.hpp"

namespace ente::judgment {

// Domain-neutral baseline: it reasons only from explicit epistemic status and
// direct same-subject value conflicts. Domain semantics require an injected
// JudgmentEngine implementation.
class StatusJudgmentEngine final : public JudgmentEngine {
public:
    [[nodiscard]] JudgmentReport evaluate_compatibility(
        const epistemic::Interpretation& current,
        const std::vector<epistemic::Observation>& new_evidence
    ) const noexcept override;
};

} // namespace ente::judgment
