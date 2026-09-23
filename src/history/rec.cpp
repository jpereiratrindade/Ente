#include "ente/history/rec.hpp"
#include "ente/core/hash.hpp"
#include <format>
#include <numeric>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <filesystem>
#include <charconv>

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace ente::history {

std::string RecoverableHistory::compute_event_hash_string(const HistoryEvent& ev) noexcept {
    std::string causal_concat;
    for (const auto& c : ev.causal_predecessors) {
        causal_concat += c.view();
        causal_concat += ",";
    }

    std::string evidence_concat;
    for (const auto& e : ev.evidence_refs) {
        evidence_concat += e.view();
        evidence_concat += ",";
    }

    return std::format("{}:{}:{}:{}:{}:{}:{}:{}:{}:{}",
        ev.id.view(),
        to_string(ev.kind),
        ev.identity.view(),
        ev.logical_time,
        ev.previous_event_digest.value,
        causal_concat,
        evidence_concat,
        ev.authority_id,
        ev.authority_epoch,
        ev.payload_digest.value
    );
}

core::Digest RecoverableHistory::head_digest() const noexcept {
    if (events_.empty()) {
        return core::Digest();
    }
    return events_.back().event_digest;
}

HistoryEvent RecoverableHistory::create_event(
    EventKind kind,
    const core::IdentityId& identity,
    core::LogicalTime time,
    std::vector<core::EventId> causal_predecessors,
    std::vector<core::EvidenceId> evidence_refs,
    std::string payload,
    std::string authority_id,
    std::string authority_epoch
) const noexcept {
    core::EventId ev_id(std::format("E{:04d}", events_.size()));
    core::Digest prev_digest = head_digest();
    core::Digest p_digest = core::HashUtil::sha256(payload);

    HistoryEvent ev{
        .id = ev_id,
        .kind = kind,
        .identity = identity,
        .logical_time = time,
        .previous_event_digest = prev_digest,
        .causal_predecessors = std::move(causal_predecessors),
        .evidence_refs = std::move(evidence_refs),
        .authority_id = std::move(authority_id),
        .authority_epoch = std::move(authority_epoch),
        .payload_content = std::move(payload),
        .payload_digest = p_digest,
        .event_digest = core::Digest()
    };

    std::string hash_input = compute_event_hash_string(ev);
    ev.event_digest = core::HashUtil::sha256(hash_input);

    return ev;
}

void RecoverableHistory::rebuild_index() noexcept {
    event_index_.clear();
    for (size_t i = 0; i < events_.size(); ++i) {
        event_index_[events_[i].id.value] = i;
    }
}

std::optional<HistoryEvent> RecoverableHistory::find_event(const core::EventId& id) const noexcept {
    auto it = event_index_.find(id.value);
    if (it != event_index_.end()) {
        return events_[it->second];
    }
    return std::nullopt;
}

bool RecoverableHistory::contains_event(const core::EventId& id) const noexcept {
    return event_index_.contains(id.value);
}

std::expected<void, core::EnteError> RecoverableHistory::append(HistoryEvent event) noexcept {
    // 0. Verify uniqueness of EventId (C10/C12 invariant)
    if (contains_event(event.id)) {
        return std::unexpected(core::EnteError::DuplicateEventId);
    }

    // 1. Verify chronological progression
    if (!events_.empty()) {
        if (event.logical_time < events_.back().logical_time) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }
        if (event.previous_event_digest != events_.back().event_digest) {
            return std::unexpected(core::EnteError::HistoryGap);
        }
    } else {
        if (!event.previous_event_digest.is_zero()) {
            return std::unexpected(core::EnteError::GenesisNotEstablished);
        }
    }

    // 2. Verify payload digest
    core::Digest computed_payload = core::HashUtil::sha256(event.payload_content);
    if (computed_payload != event.payload_digest) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }

    // 3. Verify event digest
    std::string hash_input = compute_event_hash_string(event);
    core::Digest computed_event_digest = core::HashUtil::sha256(hash_input);
    if (computed_event_digest != event.event_digest) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }

    // 4. Verify RIT causal links exist in history via O(1) indexed lookup
    for (const auto& causal_id : event.causal_predecessors) {
        if (!contains_event(causal_id)) {
            return std::unexpected(core::EnteError::InvalidPredecessor);
        }
    }

    event_index_[event.id.value] = events_.size();
    events_.push_back(std::move(event));
    return {};
}

bool RecoverableHistory::verify_integrity() const noexcept {
    if (events_.empty()) {
        return true;
    }

    core::Digest expected_prev; // Starts at zero
    std::unordered_set<std::string> seen_event_ids;

    for (size_t i = 0; i < events_.size(); ++i) {
        const auto& ev = events_[i];

        // 0. Verify EventId uniqueness
        if (seen_event_ids.contains(ev.id.value)) {
            return false;
        }
        seen_event_ids.insert(ev.id.value);

        // 1. Check previous event linking
        if (ev.previous_event_digest != expected_prev) {
            return false;
        }

        // 2. Verify payload integrity
        core::Digest payload_hash = core::HashUtil::sha256(ev.payload_content);
        if (payload_hash != ev.payload_digest) {
            return false;
        }

        // 3. Verify event hash
        std::string hash_input = compute_event_hash_string(ev);
        core::Digest event_hash = core::HashUtil::sha256(hash_input);
        if (event_hash != ev.event_digest) {
            return false;
        }

        // 4. Verify chronological order
        if (i > 0 && ev.logical_time < events_[i - 1].logical_time) {
            return false;
        }

        expected_prev = ev.event_digest;
    }

    return true;
}

bool RecoverableHistory::verify_tail() const noexcept {
    if (events_.empty()) return true;

    const auto& tail = events_.back();
    const core::Digest expected_previous = events_.size() == 1
        ? core::Digest()
        : events_[events_.size() - 2].event_digest;
    if (tail.previous_event_digest != expected_previous) return false;
    if (events_.size() > 1 && tail.logical_time < events_[events_.size() - 2].logical_time) return false;
    if (core::HashUtil::sha256(tail.payload_content) != tail.payload_digest) return false;
    if (core::HashUtil::sha256(compute_event_hash_string(tail)) != tail.event_digest) return false;

    for (const auto& causal_id : tail.causal_predecessors) {
        auto it = event_index_.find(causal_id.value);
        if (it == event_index_.end() || it->second >= events_.size() - 1) return false;
    }
    return true;
}


void RecoverableHistory::tamper_event_payload_for_testing(size_t index, std::string_view corrupted_payload) noexcept {
    if (index < events_.size()) {
        events_[index].payload_content = corrupted_payload;
    }
}

namespace {

std::string escape_payload(std::string_view raw) {
    std::string out;
    out.reserve(raw.size() + 16);
    for (char c : raw) {
        if (c == '\\') {
            out += "\\\\";
        } else if (c == '|') {
            out += "\\|";
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else {
            out += c;
        }
    }
    return out;
}

std::string unescape_payload(std::string_view esc) {
    std::string out;
    out.reserve(esc.size());
    for (size_t i = 0; i < esc.size(); ++i) {
        if (esc[i] == '\\' && i + 1 < esc.size()) {
            char next = esc[i + 1];
            if (next == '\\') { out += '\\'; ++i; }
            else if (next == '|') { out += '|'; ++i; }
            else if (next == 'n') { out += '\n'; ++i; }
            else if (next == 'r') { out += '\r'; ++i; }
            else { out += esc[i]; }
        } else {
            out += esc[i];
        }
    }
    return out;
}

bool sync_path_to_storage(const std::string& path) noexcept {
#if defined(__unix__) || defined(__APPLE__)
    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) return false;
    const bool synced = ::fsync(fd) == 0;
    const bool closed = ::close(fd) == 0;
    return synced && closed;
#else
    // Standard C++ has no portable fsync primitive. The atomic replacement is
    // still used, but platform adapters must provide equivalent durability.
    (void)path;
    return true;
#endif
}

bool sync_parent_directory(const std::string& path) noexcept {
#if defined(__unix__) || defined(__APPLE__)
    try {
        std::filesystem::path parent = std::filesystem::path(path).parent_path();
        if (parent.empty()) parent = ".";
        const int fd = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY);
        if (fd < 0) return false;
        const bool synced = ::fsync(fd) == 0;
        const bool closed = ::close(fd) == 0;
        return synced && closed;
    } catch (...) {
        return false;
    }
#else
    (void)path;
    return true;
#endif
}

bool operation_enabled(
    const PersistenceOptions& options,
    PersistenceOperation operation
) noexcept {
    return options.before_operation == nullptr ||
        options.before_operation(operation, options.context);
}

std::string serialize_event_records(const std::vector<HistoryEvent>& events) {
    std::ostringstream output;
    for (const auto& ev : events) {
        std::string causal_str;
        for (size_t i = 0; i < ev.causal_predecessors.size(); ++i) {
            causal_str += ev.causal_predecessors[i].view();
            if (i + 1 < ev.causal_predecessors.size()) causal_str += ",";
        }

        std::string evidence_str;
        for (size_t i = 0; i < ev.evidence_refs.size(); ++i) {
            evidence_str += ev.evidence_refs[i].view();
            if (i + 1 < ev.evidence_refs.size()) evidence_str += ",";
        }

        output << ev.id.view() << "|"
               << to_string(ev.kind) << "|"
               << ev.identity.view() << "|"
               << ev.logical_time << "|"
               << ev.previous_event_digest.value << "|"
               << ev.payload_digest.value << "|"
               << ev.event_digest.value << "|"
               << ev.authority_id << "|"
               << ev.authority_epoch << "|"
               << causal_str << "|"
               << evidence_str << "|"
               << escape_payload(ev.payload_content) << "\n";
    }
    return output.str();
}

std::optional<EventKind> parse_event_kind(std::string_view kind) noexcept {
    if (kind == "GENESIS") return EventKind::Genesis;
    if (kind == "OBSERVATION") return EventKind::Observation;
    if (kind == "INTERPRETATION") return EventKind::Interpretation;
    if (kind == "PERTURBATION") return EventKind::Perturbation;
    if (kind == "JUDGMENT") return EventKind::Judgment;
    if (kind == "ACTION_AUTHORIZED") return EventKind::ActionAuthorized;
    if (kind == "ACTION_INTENDED") return EventKind::ActionIntended;
    if (kind == "ACTION_EXECUTION") return EventKind::ActionExecution;
    if (kind == "ACTION_EXECUTION_ACK") return EventKind::ActionExecutionAck;
    if (kind == "EFFECT_OBSERVATION") return EventKind::EffectObservation;
    if (kind == "EPISTEMIC_ACTION") return EventKind::EpistemicAction;
    if (kind == "REINTERPRETATION") return EventKind::Reinterpretation;
    if (kind == "ADAPTATION") return EventKind::Adaptation;
    if (kind == "CONSTITUTIVE_WARNING") return EventKind::ConstitutiveWarning;
    if (kind == "CONSTITUTIVE_REPAIR") return EventKind::ConstitutiveRepair;
    if (kind == "COHERENCE_RESTORED") return EventKind::CoherenceRestored;
    return std::nullopt;
}

std::expected<RecoverableHistory, core::EnteError> parse_event_records(
    std::istream& input
) {
    RecoverableHistory rec;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line.starts_with("#")) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }

        std::stringstream fields(line);
        std::string id, kind_str, identity, time_str, prev_digest, payload_digest,
            event_digest, authority_id, authority_epoch, causal_str, evidence_str,
            escaped_payload;
        if (!std::getline(fields, id, '|') ||
            !std::getline(fields, kind_str, '|') ||
            !std::getline(fields, identity, '|') ||
            !std::getline(fields, time_str, '|') ||
            !std::getline(fields, prev_digest, '|') ||
            !std::getline(fields, payload_digest, '|') ||
            !std::getline(fields, event_digest, '|') ||
            !std::getline(fields, authority_id, '|') ||
            !std::getline(fields, authority_epoch, '|') ||
            !std::getline(fields, causal_str, '|') ||
            !std::getline(fields, evidence_str, '|') ||
            !std::getline(fields, escaped_payload)) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }

        const auto kind = parse_event_kind(kind_str);
        if (!kind.has_value()) return std::unexpected(core::EnteError::HistoryCorrupt);

        core::LogicalTime logical_time = 0;
        const auto* time_end = time_str.data() + time_str.size();
        const auto [parsed_end, parse_error] = std::from_chars(
            time_str.data(),
            time_end,
            logical_time
        );
        if (parse_error != std::errc{} || parsed_end != time_end) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }

        std::vector<core::EventId> causal_predecessors;
        if (!causal_str.empty()) {
            std::stringstream causal_stream(causal_str);
            std::string item;
            while (std::getline(causal_stream, item, ',')) {
                if (!item.empty()) causal_predecessors.emplace_back(item);
            }
        }

        std::vector<core::EvidenceId> evidence_refs;
        if (!evidence_str.empty()) {
            std::stringstream evidence_stream(evidence_str);
            std::string item;
            while (std::getline(evidence_stream, item, ',')) {
                if (!item.empty()) evidence_refs.emplace_back(item);
            }
        }

        HistoryEvent event{
            .id = core::EventId(id),
            .kind = *kind,
            .identity = core::IdentityId(identity),
            .logical_time = logical_time,
            .previous_event_digest = core::Digest(prev_digest),
            .causal_predecessors = std::move(causal_predecessors),
            .evidence_refs = std::move(evidence_refs),
            .authority_id = std::move(authority_id),
            .authority_epoch = std::move(authority_epoch),
            .payload_content = unescape_payload(escaped_payload),
            .payload_digest = core::Digest(payload_digest),
            .event_digest = core::Digest(event_digest)
        };
        auto appended = rec.append(std::move(event));
        if (!appended.has_value()) return std::unexpected(appended.error());
    }

    if (rec.empty()) return std::unexpected(core::EnteError::HistoryGap);
    if (!rec.verify_integrity()) return std::unexpected(core::EnteError::HistoryCorrupt);
    return rec;
}

std::expected<void, core::EnteError> write_contents_atomically(
    std::string_view filepath,
    std::string_view contents,
    PersistenceOptions options
) noexcept {
    bool target_replaced = false;
    try {
        const std::string target_path(filepath);
        const std::string temporary_path = target_path + ".tmp";
        {
            std::ofstream output(temporary_path, std::ios::binary | std::ios::trunc);
            if (!output.is_open()) {
                return std::unexpected(core::EnteError::PersistenceFailure);
            }
            output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
            output.flush();
            if (!output.good()) {
                return std::unexpected(core::EnteError::PersistenceFailure);
            }
            output.close();
            if (!output.good()) {
                return std::unexpected(core::EnteError::PersistenceFailure);
            }
        }

        if (!operation_enabled(options, PersistenceOperation::SyncTemporaryFile) ||
            !sync_path_to_storage(temporary_path)) {
            std::error_code ignored;
            std::filesystem::remove(temporary_path, ignored);
            return std::unexpected(core::EnteError::PersistenceFailure);
        }
        if (!operation_enabled(options, PersistenceOperation::ReplaceTarget)) {
            std::error_code ignored;
            std::filesystem::remove(temporary_path, ignored);
            return std::unexpected(core::EnteError::PersistenceFailure);
        }

        std::error_code rename_error;
        std::filesystem::rename(temporary_path, target_path, rename_error);
        if (rename_error) {
            std::error_code ignored;
            std::filesystem::remove(temporary_path, ignored);
            return std::unexpected(core::EnteError::PersistenceFailure);
        }
        target_replaced = true;

        if (!operation_enabled(options, PersistenceOperation::SyncParentDirectory) ||
            !sync_parent_directory(target_path)) {
            return std::unexpected(core::EnteError::PersistenceCommitUncertain);
        }
        return {};
    } catch (...) {
        return std::unexpected(
            target_replaced
                ? core::EnteError::PersistenceCommitUncertain
                : core::EnteError::PersistenceFailure
        );
    }
}

} // namespace

std::expected<void, core::EnteError> RecoverableHistory::save_to_file(
    std::string_view filepath,
    PersistenceOptions options
) const noexcept {
    try {
        const std::string contents = "# ENTE_REC_V1\n" + serialize_event_records(events_);
        return write_contents_atomically(filepath, contents, options);
    } catch (...) {
        return std::unexpected(core::EnteError::PersistenceFailure);
    }
}

std::expected<RecoverableHistory, core::EnteError> RecoverableHistory::load_from_file(std::string_view filepath) noexcept {
    try {
        std::ifstream in(std::string(filepath), std::ios::binary);
        if (!in.is_open()) {
            return std::unexpected(core::EnteError::HistoryGap);
        }
        std::string header;
        if (!std::getline(in, header) || header != "# ENTE_REC_V1") {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }
        return parse_event_records(in);
    } catch (...) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }
}

std::expected<void, core::EnteError> RecoverableHistory::save_authenticated_to_file(
    std::string_view filepath,
    const core::Ed25519KeyPair& signer,
    PersistenceOptions options
) const noexcept {
    try {
        const std::string body = serialize_event_records(events_);
        const std::string signed_message = "ENTE_REC_V2\n" + body;
        const auto signature = signer.sign_hex(signed_message);
        if (!signature.has_value()) return std::unexpected(signature.error());

        const std::string contents =
            "# ENTE_REC_V2\n# ED25519_SIGNATURE=" + *signature + "\n" + body;
        return write_contents_atomically(filepath, contents, options);
    } catch (...) {
        return std::unexpected(core::EnteError::CryptographicFailure);
    }
}

std::expected<RecoverableHistory, core::EnteError> RecoverableHistory::load_authenticated_from_file(
    std::string_view filepath,
    std::string_view trusted_public_key_hex
) noexcept {
    try {
        std::ifstream input(std::string(filepath), std::ios::binary);
        if (!input.is_open()) return std::unexpected(core::EnteError::HistoryGap);

        const std::string contents{
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()
        };
        constexpr std::string_view header = "# ENTE_REC_V2\n";
        constexpr std::string_view signature_prefix = "# ED25519_SIGNATURE=";
        if (!contents.starts_with(header)) {
            return std::unexpected(core::EnteError::InvalidSignature);
        }

        const size_t signature_line_start = header.size();
        const size_t signature_line_end = contents.find('\n', signature_line_start);
        if (signature_line_end == std::string::npos) {
            return std::unexpected(core::EnteError::InvalidSignature);
        }
        const std::string_view signature_line(contents.data() + signature_line_start,
                                              signature_line_end - signature_line_start);
        if (!signature_line.starts_with(signature_prefix)) {
            return std::unexpected(core::EnteError::InvalidSignature);
        }
        const std::string_view signature = signature_line.substr(signature_prefix.size());
        const std::string_view body(contents.data() + signature_line_end + 1,
                                    contents.size() - signature_line_end - 1);
        const std::string signed_message = "ENTE_REC_V2\n" + std::string(body);
        if (!core::Ed25519KeyPair::verify_hex(
                trusted_public_key_hex,
                signed_message,
                signature)) {
            return std::unexpected(core::EnteError::InvalidSignature);
        }

        std::istringstream records{std::string(body)};
        return parse_event_records(records);
    } catch (...) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }
}

} // namespace ente::history
