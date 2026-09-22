#pragma once

#include "ente/constitution/invariants.hpp"
#include "ente/identity/identity.hpp"
#include "ente/history/rec.hpp"
#include "ente/epistemic/interpretation.hpp"
#include <vector>
#include <map>

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

    [[nodiscard]] VerificationReport verify(
        const identity::IdentityState& identity,
        const std::optional<identity::GenesisRecord>& genesis,
        const history::RecoverableHistory& history,
        const std::optional<epistemic::Interpretation>& current_interpretation
    ) const noexcept;
};

} // namespace ente::constitution
