#include "ente/history/payloads.hpp"

#include <array>
#include <charconv>

namespace ente::history {

namespace {

constexpr std::string_view prefix = "ENTE_ACTION_TX_V1";

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
    std::string output(prefix);
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
        if (!serialized.starts_with(prefix)) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }

        size_t cursor = prefix.size();
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

} // namespace ente::history
