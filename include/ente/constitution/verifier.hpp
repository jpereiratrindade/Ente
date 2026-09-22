#pragma once

#include "ente/constitution/invariants.hpp"
#include "ente/identity/identity.hpp"
#include "ente/history/rec.hpp"
#include "ente/epistemic/interpretation.hpp"
#include <vector>
#include <map>

#include "ente/authority/authority.hpp"
#include <functional>

namespace ente::constitution {

enum class ConstitutiveStatus : uint8_t {
    Valid,
    Weakened,
    Suspended,
    Violated
};

[[nodiscard]] constexpr std::string_view to_string(ConstitutiveStatus s) noexcept {
    switch (s) {
        case ConstitutiveStatus::Valid: return "VALID";
        case ConstitutiveStatus::Weakened: return "WEAKENED";
        case ConstitutiveStatus::Suspended: return "SUSPENDED";
        case ConstitutiveStatus::Violated: return "VIOLATED";
    }
    return "UNKNOWN_STATUS";
}

struct InvariantReport {
    InvariantId id;
    InvariantStatus status;
    std::string details;
};

struct VerificationReport {
    ConstitutiveStatus status;
    std::vector<InvariantReport> invariant_reports;

    [[nodiscard]] bool is_valid() const noexcept {
        return status == ConstitutiveStatus::Valid;
    }
};

class ConstitutionVerifier {
public:
    ConstitutionVerifier() = default;

    // Full historical audit (O(H)) from Genesis to HEAD
    [[nodiscard]] VerificationReport verify(
        const identity::IdentityState& identity,
        const std::optional<identity::GenesisRecord>& genesis,
        const history::RecoverableHistory& history,
        const std::optional<epistemic::Interpretation>& current_interpretation,
        std::optional<std::reference_wrapper<const authority::AuthorityLineage>> authority_lineage = std::nullopt
    ) const noexcept;

    // Incremental step-level verification (O(1)) for continuous operation
    [[nodiscard]] VerificationReport verify_step(
        const identity::IdentityState& identity,
        const std::optional<identity::GenesisRecord>& genesis,
        const history::HistoryEvent& latest_event,
        const std::optional<epistemic::Interpretation>& current_interpretation,
        std::optional<std::reference_wrapper<const authority::AuthorityLineage>> authority_lineage = std::nullopt
    ) const noexcept;
};

} // namespace ente::constitution
