#pragma once

#include "ente/core/types.hpp"
#include "ente/epistemic/status.hpp"
#include <string>

namespace ente::epistemic {

struct Observation {
    core::EvidenceId id;
    std::string source;
    std::string subject;
    std::string value;
    core::LogicalTime observed_at{0};
    EpistemicStatus status{EpistemicStatus::Observed};
};

} // namespace ente::epistemic
