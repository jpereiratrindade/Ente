#pragma once

#include "ente/core/types.hpp"
#include <string>
#include <vector>
#include <string_view>

namespace ente::epistemic {

enum class EvidenceDemandPurpose : uint8_t {
    DiscriminateContradiction,
    ResolveUnknown,
    CorroborateInference,
    VerifySensorCalibration
};

[[nodiscard]] constexpr std::string_view to_string(EvidenceDemandPurpose p) noexcept {
    switch (p) {
        case EvidenceDemandPurpose::DiscriminateContradiction: return "DISCRIMINATE_CONTRADICTION";
        case EvidenceDemandPurpose::ResolveUnknown: return "RESOLVE_UNKNOWN";
        case EvidenceDemandPurpose::CorroborateInference: return "CORROBORATE_INFERENCE";
        case EvidenceDemandPurpose::VerifySensorCalibration: return "VERIFY_SENSOR_CALIBRATION";
    }
    return "UNKNOWN_PURPOSE";
}

struct EvidenceRequest {
    std::string subject;
    EvidenceDemandPurpose purpose{EvidenceDemandPurpose::DiscriminateContradiction};
    std::vector<std::string> conflicting_sources;
    std::string suggested_discriminator;
    std::string demand_summary;
};

} // namespace ente::epistemic
