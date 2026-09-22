#pragma once

#include "ente/core/types.hpp"
#include "ente/epistemic/status.hpp"
#include <vector>
#include <string>
#include <optional>

namespace ente::epistemic {

enum class InterpretationStatus : uint8_t {
    Current,
    Supported,
    Weakened,
    Superseded,
    Contradicted,
    Unknown
};

[[nodiscard]] constexpr std::string_view to_string(InterpretationStatus s) noexcept {
    switch (s) {
        case InterpretationStatus::Current: return "CURRENT";
        case InterpretationStatus::Supported: return "SUPPORTED";
        case InterpretationStatus::Weakened: return "WEAKENED";
        case InterpretationStatus::Superseded: return "SUPERSEDED";
        case InterpretationStatus::Contradicted: return "CONTRADICTED";
        case InterpretationStatus::Unknown: return "UNKNOWN";
    }
    return "INVALID_INTERPRETATION_STATUS";
}

struct Interpretation {
    core::InterpretationId id;
    std::string subject;
    std::string proposition;
    
    std::vector<core::EvidenceId> supporting_evidence;
    std::vector<core::EvidenceId> challenging_evidence;
    std::optional<core::InterpretationId> supersedes; // RIT lineage
    
    InterpretationStatus status{InterpretationStatus::Current};
    core::LogicalTime created_at{0};
};

} // namespace ente::epistemic
