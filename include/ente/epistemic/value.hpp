#pragma once

#include "ente/core/types.hpp"
#include "ente/epistemic/status.hpp"
#include <optional>
#include <string>
#include <format>

namespace ente::epistemic {

// Strong Epistemic Value encapsulating data with its epistemic status and provenance (C4, C5, C8)
template <typename T>
struct EpistemicValue {
    EpistemicStatus status{EpistemicStatus::Unknown};
    std::optional<T> value{std::nullopt};
    core::EvidenceId evidence_origin{};
    std::string qualification{}; // Explicação adicional quando Unknown ou Contradictory

    [[nodiscard]] constexpr bool is_unknown() const noexcept {
        return status == EpistemicStatus::Unknown;
    }

    [[nodiscard]] constexpr bool is_observed() const noexcept {
        return status == EpistemicStatus::Observed;
    }

    [[nodiscard]] constexpr bool is_contradictory() const noexcept {
        return status == EpistemicStatus::Contradictory;
    }

    [[nodiscard]] static EpistemicValue<T> make_observed(T val, core::EvidenceId ev_id) noexcept {
        return EpistemicValue{
            .status = EpistemicStatus::Observed,
            .value = std::move(val),
            .evidence_origin = ev_id,
            .qualification = "Direct observation"
        };
    }

    [[nodiscard]] static EpistemicValue<T> make_unknown(core::EvidenceId ev_id, std::string reason = "Unclassified novelty") noexcept {
        return EpistemicValue{
            .status = EpistemicStatus::Unknown,
            .value = std::nullopt,
            .evidence_origin = ev_id,
            .qualification = std::move(reason)
        };
    }

    [[nodiscard]] static EpistemicValue<T> make_contradictory(core::EvidenceId ev_id, std::string conflict_desc) noexcept {
        return EpistemicValue{
            .status = EpistemicStatus::Contradictory,
            .value = std::nullopt,
            .evidence_origin = ev_id,
            .qualification = std::move(conflict_desc)
        };
    }
};

} // namespace ente::epistemic
