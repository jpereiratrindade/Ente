#include "ente/history/payloads.hpp"

#include <array>
#include <charconv>

namespace ente::history {

namespace {

constexpr std::string_view action_prefix = "ENTE_ACTION_TX_V1";
constexpr std::string_view genesis_prefix = "ENTE_GENESIS_V1";
constexpr std::string_view observation_prefix = "ENTE_OBSERVATION_V1";
constexpr std::string_view interpretation_prefix = "ENTE_INTERPRETATION_V2";
constexpr std::string_view migration_prefix = "ENTE_MATERIAL_MIGRATION_V1";
constexpr std::string_view judgment_prefix = "ENTE_JUDGMENT_V1";
constexpr std::string_view perturbation_prefix = "ENTE_PERTURBATION_V1";
constexpr std::string_view epistemic_action_prefix = "ENTE_EPISTEMIC_ACTION_V1";
constexpr std::string_view assurance_prefix = "ENTE_ASSURANCE_DECISION_V1";
constexpr std::string_view authority_transition_prefix = "ENTE_AUTHORITY_TRANSITION_V1";
constexpr std::string_view constitutive_event_prefix = "ENTE_CONSTITUTIVE_EVENT_V1";

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

std::optional<epistemic::InterpretationStatus> parse_interpretation_status(
    std::string_view value
) noexcept {
    if (value == "CURRENT") return epistemic::InterpretationStatus::Current;
    if (value == "SUPPORTED") return epistemic::InterpretationStatus::Supported;
    if (value == "WEAKENED") return epistemic::InterpretationStatus::Weakened;
    if (value == "SUPERSEDED") return epistemic::InterpretationStatus::Superseded;
    if (value == "CONTRADICTED") return epistemic::InterpretationStatus::Contradicted;
    if (value == "UNKNOWN") return epistemic::InterpretationStatus::Unknown;
    return std::nullopt;
}

std::optional<rcc::EpistemicAction> parse_epistemic_action(std::string_view value) noexcept {
    if (value == "KEEP") return rcc::EpistemicAction::Keep;
    if (value == "REOBSERVE") return rcc::EpistemicAction::Reobserve;
    if (value == "SEEK_EVIDENCE") return rcc::EpistemicAction::SeekEvidence;
    if (value == "SUSPEND_ACTION") return rcc::EpistemicAction::SuspendAction;
    if (value == "REINTERPRET") return rcc::EpistemicAction::Reinterpret;
    if (value == "SUSPEND_JUDGMENT") return rcc::EpistemicAction::SuspendJudgment;
    return std::nullopt;
}

std::optional<assurance::SafetyDirective> parse_safety_directive(
    std::string_view value
) noexcept {
    return parse_directive(value);
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

std::string serialize_evidence_ids(const std::vector<core::EvidenceId>& ids) {
    std::string result = std::to_string(ids.size());
    for (const auto& id : ids) append_field(result, id.view());
    return result;
}

std::expected<std::vector<core::EvidenceId>, core::EnteError> parse_evidence_ids(
    std::string_view serialized
) noexcept {
    try {
        const auto separator = serialized.find('|');
        const auto count_text = serialized.substr(0, separator);
        size_t count = 0;
        const auto [end, error] = std::from_chars(
            count_text.data(), count_text.data() + count_text.size(), count);
        if (count_text.empty() || error != std::errc{} ||
            end != count_text.data() + count_text.size()) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }
        size_t cursor = separator == std::string_view::npos
            ? serialized.size()
            : separator;
        std::vector<core::EvidenceId> result;
        result.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            auto value = read_field(serialized, cursor);
            if (!value.has_value() || value->empty()) {
                return std::unexpected(core::EnteError::HistoryCorrupt);
            }
            result.emplace_back(*value);
        }
        if (cursor != serialized.size()) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }
        return result;
    } catch (...) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }
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
    append_field(output, payload.root_authority_public_key);
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
        std::array<std::string_view, 6> fields;
        for (auto& field : fields) {
            auto result = read_field(serialized, cursor);
            if (!result.has_value()) return std::unexpected(result.error());
            field = *result;
        }
        if (cursor != serialized.size() || fields[0].empty() || fields[2].empty() ||
            fields[3].empty() || fields[4].empty() || fields[5].empty()) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }
        return GenesisPayload{
            .identity = core::IdentityId(fields[0]),
            .genesis_digest = core::Digest(fields[1]),
            .material_anchor_id = std::string(fields[2]),
            .hardware_fingerprint = std::string(fields[3]),
            .substrate_type = std::string(fields[4]),
            .root_authority_public_key = std::string(fields[5])
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
    append_field(output, epistemic::to_string(payload.status));
    append_field(output, serialize_evidence_ids(payload.supporting_evidence));
    append_field(output, serialize_evidence_ids(payload.challenging_evidence));
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
        std::array<std::string_view, 8> fields;
        for (auto& field : fields) {
            auto result = read_field(serialized, cursor);
            if (!result.has_value()) return std::unexpected(result.error());
            field = *result;
        }
        const auto time = parse_logical_time(fields[3]);
        const auto status = parse_interpretation_status(fields[5]);
        const auto supporting = parse_evidence_ids(fields[6]);
        const auto challenging = parse_evidence_ids(fields[7]);
        if (cursor != serialized.size() || fields[0].empty() || fields[1].empty() ||
            !time.has_value() || !status.has_value() || !supporting.has_value() ||
            !challenging.has_value()) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }
        return InterpretationPayload{
            .id = core::InterpretationId(fields[0]),
            .subject = std::string(fields[1]),
            .proposition = std::string(fields[2]),
            .supporting_evidence = *supporting,
            .challenging_evidence = *challenging,
            .supersedes = fields[4].empty()
                ? std::nullopt
                : std::optional<core::InterpretationId>(core::InterpretationId(fields[4])),
            .status = *status,
            .created_at = *time
        };
    } catch (...) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }
}

std::string serialize_judgment_payload(const JudgmentPayload& payload) {
    std::string output(judgment_prefix);
    append_field(output, payload.compatibility);
    append_field(output, payload.rationale);
    append_field(output, payload.engine_digest.value);
    return output;
}

std::expected<JudgmentPayload, core::EnteError> parse_judgment_payload(
    std::string_view serialized
) noexcept {
    try {
        if (!serialized.starts_with(judgment_prefix)) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }
        size_t cursor = judgment_prefix.size();
        std::array<std::string_view, 3> fields;
        for (auto& field : fields) {
            auto result = read_field(serialized, cursor);
            if (!result.has_value()) return std::unexpected(result.error());
            field = *result;
        }
        if (cursor != serialized.size() || fields[0].empty() || fields[2].empty()) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }
        return JudgmentPayload{
            .compatibility = std::string(fields[0]),
            .rationale = std::string(fields[1]),
            .engine_digest = core::Digest(fields[2])
        };
    } catch (...) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }
}

std::string serialize_perturbation_payload(const PerturbationPayload& payload) {
    std::string output(perturbation_prefix);
    append_field(output, payload.compatibility);
    append_field(output, payload.reason);
    append_field(output, rcc::to_string(payload.recommended_action));
    return output;
}

std::expected<PerturbationPayload, core::EnteError> parse_perturbation_payload(
    std::string_view serialized
) noexcept {
    try {
        if (!serialized.starts_with(perturbation_prefix)) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }
        size_t cursor = perturbation_prefix.size();
        std::array<std::string_view, 3> fields;
        for (auto& field : fields) {
            auto result = read_field(serialized, cursor);
            if (!result.has_value()) return std::unexpected(result.error());
            field = *result;
        }
        const auto action = parse_epistemic_action(fields[2]);
        if (cursor != serialized.size() || fields[0].empty() || !action.has_value()) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }
        return PerturbationPayload{
            .compatibility = std::string(fields[0]),
            .reason = std::string(fields[1]),
            .recommended_action = *action
        };
    } catch (...) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }
}

std::string serialize_epistemic_action_payload(const EpistemicActionPayload& payload) {
    std::string output(epistemic_action_prefix);
    append_field(output, rcc::to_string(payload.action));
    append_field(output, payload.reason);
    return output;
}

std::expected<EpistemicActionPayload, core::EnteError> parse_epistemic_action_payload(
    std::string_view serialized
) noexcept {
    try {
        if (!serialized.starts_with(epistemic_action_prefix)) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }
        size_t cursor = epistemic_action_prefix.size();
        std::array<std::string_view, 2> fields;
        for (auto& field : fields) {
            auto result = read_field(serialized, cursor);
            if (!result.has_value()) return std::unexpected(result.error());
            field = *result;
        }
        const auto action = parse_epistemic_action(fields[0]);
        if (cursor != serialized.size() || !action.has_value()) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }
        return EpistemicActionPayload{.action = *action, .reason = std::string(fields[1])};
    } catch (...) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }
}

std::string serialize_assurance_decision_payload(const AssuranceDecisionPayload& payload) {
    std::string output(assurance_prefix);
    append_field(output, assurance::to_string(payload.directive));
    append_field(output, rcc::to_string(payload.epistemic_action));
    append_field(output, payload.constitutive_status);
    return output;
}

std::expected<AssuranceDecisionPayload, core::EnteError> parse_assurance_decision_payload(
    std::string_view serialized
) noexcept {
    try {
        if (!serialized.starts_with(assurance_prefix)) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }
        size_t cursor = assurance_prefix.size();
        std::array<std::string_view, 3> fields;
        for (auto& field : fields) {
            auto result = read_field(serialized, cursor);
            if (!result.has_value()) return std::unexpected(result.error());
            field = *result;
        }
        const auto directive = parse_safety_directive(fields[0]);
        const auto action = parse_epistemic_action(fields[1]);
        if (cursor != serialized.size() || !directive.has_value() ||
            !action.has_value() || fields[2].empty()) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }
        return AssuranceDecisionPayload{
            .directive = *directive,
            .epistemic_action = *action,
            .constitutive_status = std::string(fields[2])
        };
    } catch (...) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }
}

std::string serialize_authority_transition_payload(const AuthorityTransitionPayload& payload) {
    std::string output(authority_transition_prefix);
    append_field(output, payload.new_epoch_id);
    append_field(output, payload.previous_epoch_id);
    append_field(output, payload.new_authority_id);
    append_field(output, payload.new_authority_public_key);
    append_field(output, std::to_string(payload.transition_time));
    append_field(output, payload.predecessor_epoch_digest.value);
    append_field(output, payload.delegation_signature);
    append_field(output, payload.delegation_policy);
    return output;
}

std::expected<AuthorityTransitionPayload, core::EnteError> parse_authority_transition_payload(
    std::string_view serialized
) noexcept {
    try {
        if (!serialized.starts_with(authority_transition_prefix)) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }
        size_t cursor = authority_transition_prefix.size();
        std::array<std::string_view, 8> fields;
        for (auto& field : fields) {
            auto result = read_field(serialized, cursor);
            if (!result.has_value()) return std::unexpected(result.error());
            field = *result;
        }
        const auto time = parse_logical_time(fields[4]);
        if (cursor != serialized.size() || fields[0].empty() || fields[1].empty() ||
            fields[2].empty() || fields[3].empty() || !time.has_value() ||
            fields[5].empty() || fields[6].empty() || fields[7] != "STRICT_LINEAGE") {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }
        return AuthorityTransitionPayload{
            .new_epoch_id = std::string(fields[0]),
            .previous_epoch_id = std::string(fields[1]),
            .new_authority_id = std::string(fields[2]),
            .new_authority_public_key = std::string(fields[3]),
            .transition_time = *time,
            .predecessor_epoch_digest = core::Digest(fields[5]),
            .delegation_signature = std::string(fields[6]),
            .delegation_policy = std::string(fields[7])
        };
    } catch (...) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }
}

std::string serialize_constitutive_event_payload(const ConstitutiveEventPayload& payload) {
    std::string output(constitutive_event_prefix);
    append_field(output, payload.code);
    append_field(output, payload.detail);
    return output;
}

std::expected<ConstitutiveEventPayload, core::EnteError> parse_constitutive_event_payload(
    std::string_view serialized
) noexcept {
    try {
        if (!serialized.starts_with(constitutive_event_prefix)) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }
        size_t cursor = constitutive_event_prefix.size();
        std::array<std::string_view, 2> fields;
        for (auto& field : fields) {
            auto result = read_field(serialized, cursor);
            if (!result.has_value()) return std::unexpected(result.error());
            field = *result;
        }
        if (cursor != serialized.size() || fields[0].empty()) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }
        return ConstitutiveEventPayload{
            .code = std::string(fields[0]),
            .detail = std::string(fields[1])
        };
    } catch (...) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }
}

bool payload_matches_event_kind(EventKind kind, std::string_view serialized) noexcept {
    switch (kind) {
        case EventKind::Genesis:
            return parse_genesis_payload(serialized).has_value();
        case EventKind::Observation:
            return parse_observation_payload(serialized).has_value();
        case EventKind::Interpretation:
        case EventKind::Reinterpretation:
        case EventKind::CoherenceRestored:
            return parse_interpretation_payload(serialized).has_value();
        case EventKind::Perturbation:
            return parse_perturbation_payload(serialized).has_value();
        case EventKind::Judgment:
            return parse_judgment_payload(serialized).has_value();
        case EventKind::ActionAuthorized:
            return parse_assurance_decision_payload(serialized).has_value() ||
                parse_action_transaction(serialized).has_value();
        case EventKind::ActionIntended:
        case EventKind::ActionExecution:
        case EventKind::ActionExecutionAck:
        case EventKind::EffectObservation:
            return parse_action_transaction(serialized).has_value();
        case EventKind::EpistemicAction:
            return parse_epistemic_action_payload(serialized).has_value();
        case EventKind::Adaptation:
            return parse_material_migration_payload(serialized).has_value();
        case EventKind::AuthorityTransition:
            return parse_authority_transition_payload(serialized).has_value();
        case EventKind::ConstitutiveWarning:
            return parse_constitutive_event_payload(serialized).has_value() ||
                parse_action_transaction(serialized).has_value();
        case EventKind::ConstitutiveRepair:
            return parse_constitutive_event_payload(serialized).has_value();
    }
    return false;
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
