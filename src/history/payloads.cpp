#include "ente/history/payloads.hpp"

#include <array>
#include <charconv>

namespace ente::history {

namespace {

constexpr std::string_view action_prefix = "ENTE_ACTION_TX_V1";
constexpr std::string_view genesis_prefix = "ENTE_GENESIS_V1";
constexpr std::string_view observation_prefix = "ENTE_OBSERVATION_V1";
constexpr std::string_view interpretation_prefix = "ENTE_INTERPRETATION_V1";
constexpr std::string_view migration_prefix = "ENTE_MATERIAL_MIGRATION_V1";

void append_field(std::string& output, std::string_view value) {
    output += '|';
    output += std::to_string(value.size());
    output += ':';
    output += value;
}

std::optional<ActionPhase> parse_phase(std::string_view value) noexcept {
    if (value == "PREPARED") return ActionPhase::Prepared;
    if (value == "AUTHORIZED") return ActionPhase::Authorized;
    if (value == "DISPATCHED") return ActionPhase::Dispatched;
    if (value == "ACKNOWLEDGED") return ActionPhase::Acknowledged;
    if (value == "EFFECT_UNCONFIRMED") return ActionPhase::EffectUnconfirmed;
    if (value == "CONFIRMED") return ActionPhase::Confirmed;
    if (value == "FAILED") return ActionPhase::Failed;
    if (value == "RECOVERY_REQUIRED") return ActionPhase::RecoveryRequired;
    return std::nullopt;
}

std::optional<assurance::SafetyDirective> parse_directive(std::string_view value) noexcept {
    if (value == "ALLOW_ACTION") return assurance::SafetyDirective::AllowAction;
    if (value == "SAFE_HOLD") return assurance::SafetyDirective::SafeHold;
    if (value == "DEGRADE_PERFORMANCE") return assurance::SafetyDirective::DegradePerformance;
    if (value == "EMERGENCY_STOP") return assurance::SafetyDirective::EmergencyStop;
    return std::nullopt;
}

std::optional<epistemic::EpistemicStatus> parse_epistemic_status(std::string_view value) noexcept {
    if (value == "OBSERVED") return epistemic::EpistemicStatus::Observed;
    if (value == "DERIVED") return epistemic::EpistemicStatus::Derived;
    if (value == "INFERRED") return epistemic::EpistemicStatus::Inferred;
    if (value == "UNKNOWN") return epistemic::EpistemicStatus::Unknown;
    if (value == "UNCERTAIN") return epistemic::EpistemicStatus::Uncertain;
    if (value == "CONTRADICTORY") return epistemic::EpistemicStatus::Contradictory;
    return std::nullopt;
}

std::expected<core::LogicalTime, core::EnteError> parse_logical_time(
    std::string_view value
) noexcept {
    core::LogicalTime result = 0;
    const auto [end, error] = std::from_chars(
        value.data(),
        value.data() + value.size(),
        result
    );
    if (error != std::errc{} || end != value.data() + value.size()) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }
    return result;
}

std::expected<std::string_view, core::EnteError> read_field(
    std::string_view input,
    size_t& cursor
) noexcept {
    if (cursor >= input.size() || input[cursor] != '|') {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }
    ++cursor;

    const size_t colon = input.find(':', cursor);
    if (colon == std::string_view::npos || colon == cursor) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }

    size_t length = 0;
    const auto* begin = input.data() + cursor;
    const auto* end = input.data() + colon;
    const auto [ptr, error] = std::from_chars(begin, end, length);
    if (error != std::errc{} || ptr != end) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }

    cursor = colon + 1;
    if (length > input.size() - cursor) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }

    const auto value = input.substr(cursor, length);
    cursor += length;
    return value;
}

} // namespace

std::string serialize_action_transaction(const ActionTransactionPayload& payload) {
    std::string output(action_prefix);
    append_field(output, payload.action_id.view());
    append_field(output, to_string(payload.phase));
    append_field(output, payload.proposed_action);
    append_field(output, payload.effective_action);
    append_field(output, assurance::to_string(payload.safety_directive));
    append_field(output, payload.pre_state);
    append_field(output, payload.post_state);
    append_field(output, payload.detail);
    return output;
}

std::expected<ActionTransactionPayload, core::EnteError> parse_action_transaction(
    std::string_view serialized
) noexcept {
    try {
        if (!serialized.starts_with(action_prefix)) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }

        size_t cursor = action_prefix.size();
        std::array<std::string_view, 8> fields;
        for (auto& field : fields) {
            auto result = read_field(serialized, cursor);
            if (!result.has_value()) return std::unexpected(result.error());
            field = *result;
        }
        if (cursor != serialized.size()) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }

        auto phase = parse_phase(fields[1]);
        auto directive = parse_directive(fields[4]);
        if (!phase.has_value() || !directive.has_value() || fields[0].empty()) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }

        return ActionTransactionPayload{
            .action_id = core::ActionTransactionId(fields[0]),
            .phase = *phase,
            .proposed_action = std::string(fields[2]),
            .effective_action = std::string(fields[3]),
            .safety_directive = *directive,
            .pre_state = std::string(fields[5]),
            .post_state = std::string(fields[6]),
            .detail = std::string(fields[7])
        };
    } catch (...) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }
}


std::string serialize_genesis_payload(const GenesisPayload& payload) {
    std::string output(genesis_prefix);
    append_field(output, payload.identity.view());
    append_field(output, payload.genesis_digest.value);
    append_field(output, payload.material_anchor_id);
    append_field(output, payload.hardware_fingerprint);
    append_field(output, payload.substrate_type);
    return output;
}

std::expected<GenesisPayload, core::EnteError> parse_genesis_payload(
    std::string_view serialized
) noexcept {
    try {
        if (!serialized.starts_with(genesis_prefix)) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }
        size_t cursor = genesis_prefix.size();
        std::array<std::string_view, 5> fields;
        for (auto& field : fields) {
            auto result = read_field(serialized, cursor);
            if (!result.has_value()) return std::unexpected(result.error());
            field = *result;
        }
        if (cursor != serialized.size() || fields[0].empty() || fields[2].empty() ||
            fields[3].empty()) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }
        return GenesisPayload{
            .identity = core::IdentityId(fields[0]),
            .genesis_digest = core::Digest(fields[1]),
            .material_anchor_id = std::string(fields[2]),
            .hardware_fingerprint = std::string(fields[3]),
            .substrate_type = std::string(fields[4])
        };
    } catch (...) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }
}

std::string serialize_observation_payload(const ObservationPayload& payload) {
    std::string output(observation_prefix);
    append_field(output, payload.evidence_id.view());
    append_field(output, payload.source);
    append_field(output, payload.subject);
    append_field(output, payload.value);
    append_field(output, std::to_string(payload.observed_at));
    append_field(output, epistemic::to_string(payload.status));
    return output;
}

std::expected<ObservationPayload, core::EnteError> parse_observation_payload(
    std::string_view serialized
) noexcept {
    try {
        if (!serialized.starts_with(observation_prefix)) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }
        size_t cursor = observation_prefix.size();
        std::array<std::string_view, 6> fields;
        for (auto& field : fields) {
            auto result = read_field(serialized, cursor);
            if (!result.has_value()) return std::unexpected(result.error());
            field = *result;
        }
        const auto time = parse_logical_time(fields[4]);
        const auto status = parse_epistemic_status(fields[5]);
        if (cursor != serialized.size() || fields[0].empty() || fields[1].empty() ||
            fields[2].empty() || !time.has_value() || !status.has_value()) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }
        return ObservationPayload{
            .evidence_id = core::EvidenceId(fields[0]),
            .source = std::string(fields[1]),
            .subject = std::string(fields[2]),
            .value = std::string(fields[3]),
            .status = *status,
            .observed_at = *time
        };
    } catch (...) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }
}

std::string serialize_interpretation_payload(const InterpretationPayload& payload) {
    std::string output(interpretation_prefix);
    append_field(output, payload.id.view());
    append_field(output, payload.subject);
    append_field(output, payload.proposition);
    append_field(output, std::to_string(payload.created_at));
    append_field(output, payload.supersedes.has_value() ? payload.supersedes->view() : std::string_view{});
    return output;
}

std::expected<InterpretationPayload, core::EnteError> parse_interpretation_payload(
    std::string_view serialized
) noexcept {
    try {
        if (!serialized.starts_with(interpretation_prefix)) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }
        size_t cursor = interpretation_prefix.size();
        std::array<std::string_view, 5> fields;
        for (auto& field : fields) {
            auto result = read_field(serialized, cursor);
            if (!result.has_value()) return std::unexpected(result.error());
            field = *result;
        }
        const auto time = parse_logical_time(fields[3]);
        if (cursor != serialized.size() || fields[0].empty() || fields[1].empty() ||
            !time.has_value()) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }
        return InterpretationPayload{
            .id = core::InterpretationId(fields[0]),
            .subject = std::string(fields[1]),
            .proposition = std::string(fields[2]),
            .supporting_evidence = {},
            .challenging_evidence = {},
            .supersedes = fields[4].empty()
                ? std::nullopt
                : std::optional<core::InterpretationId>(core::InterpretationId(fields[4])),
            .created_at = *time
        };
    } catch (...) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }
}

std::string serialize_material_migration_payload(const MaterialMigrationPayload& payload) {
    std::string output(migration_prefix);
    append_field(output, payload.new_anchor_id);
    append_field(output, payload.new_hardware_fingerprint);
    append_field(output, payload.substrate_type);
    append_field(output, payload.previous_anchor_id.value_or(""));
    return output;
}

std::expected<MaterialMigrationPayload, core::EnteError> parse_material_migration_payload(
    std::string_view serialized
) noexcept {
    try {
        if (!serialized.starts_with(migration_prefix)) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }
        size_t cursor = migration_prefix.size();
        std::array<std::string_view, 4> fields;
        for (auto& field : fields) {
            auto result = read_field(serialized, cursor);
            if (!result.has_value()) return std::unexpected(result.error());
            field = *result;
        }
        if (cursor != serialized.size() || fields[0].empty() || fields[1].empty() ||
            fields[2].empty()) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }
        return MaterialMigrationPayload{
            .new_anchor_id = std::string(fields[0]),
            .new_hardware_fingerprint = std::string(fields[1]),
            .substrate_type = std::string(fields[2]),
            .previous_anchor_id = fields[3].empty()
                ? std::nullopt
                : std::optional<std::string>(fields[3])
        };
    } catch (...) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }
}

} // namespace ente::history
