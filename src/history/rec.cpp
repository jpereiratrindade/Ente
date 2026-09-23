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
    std::filesystem::path parent = std::filesystem::path(path).parent_path();
    if (parent.empty()) parent = ".";
    const int fd = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd < 0) return false;
    const bool synced = ::fsync(fd) == 0;
    const bool closed = ::close(fd) == 0;
    return synced && closed;
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

} // namespace

std::expected<void, core::EnteError> RecoverableHistory::save_to_file(
    std::string_view filepath,
    PersistenceOptions options
) const noexcept {
    try {
        std::string target_path(filepath);
        std::string tmp_path = target_path + ".tmp";

        {
            std::ofstream out(tmp_path, std::ios::out | std::ios::trunc);
            if (!out.is_open()) {
                return std::unexpected(core::EnteError::PersistenceFailure);
            }

            // Header for format versioning
            out << "# ENTE_REC_V1\n";

            for (const auto& ev : events_) {
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

                out << ev.id.view() << "|"
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
            out.flush();
            if (!out.good()) {
                return std::unexpected(core::EnteError::PersistenceFailure);
            }
            out.close();
            if (!out.good()) {
                return std::unexpected(core::EnteError::PersistenceFailure);
            }
        }

        if (!operation_enabled(options, PersistenceOperation::SyncTemporaryFile) ||
            !sync_path_to_storage(tmp_path)) {
            std::error_code cleanup_error;
            std::filesystem::remove(tmp_path, cleanup_error);
            return std::unexpected(core::EnteError::PersistenceFailure);
        }

        std::error_code ec;
        if (!operation_enabled(options, PersistenceOperation::ReplaceTarget)) {
            std::error_code cleanup_error;
            std::filesystem::remove(tmp_path, cleanup_error);
            return std::unexpected(core::EnteError::PersistenceFailure);
        }
        std::filesystem::rename(tmp_path, target_path, ec);
        if (ec) {
            std::error_code cleanup_error;
            std::filesystem::remove(tmp_path, cleanup_error);
            return std::unexpected(core::EnteError::PersistenceFailure);
        }

        if (!operation_enabled(options, PersistenceOperation::SyncParentDirectory) ||
            !sync_parent_directory(target_path)) {
            return std::unexpected(core::EnteError::PersistenceFailure);
        }

        return {};
    } catch (...) {
        return std::unexpected(core::EnteError::PersistenceFailure);
    }
}

std::expected<RecoverableHistory, core::EnteError> RecoverableHistory::load_from_file(std::string_view filepath) noexcept {
    try {
        std::string path_str(filepath);
        std::ifstream in(path_str);
        if (!in.is_open()) {
            return std::unexpected(core::EnteError::HistoryGap);
        }

        RecoverableHistory rec;
        std::string line;

        auto parse_kind = [](std::string_view k) -> std::optional<EventKind> {
            if (k == "GENESIS") return EventKind::Genesis;
            if (k == "OBSERVATION") return EventKind::Observation;
            if (k == "INTERPRETATION") return EventKind::Interpretation;
            if (k == "PERTURBATION") return EventKind::Perturbation;
            if (k == "JUDGMENT") return EventKind::Judgment;
            if (k == "ACTION_AUTHORIZED") return EventKind::ActionAuthorized;
            if (k == "ACTION_INTENDED") return EventKind::ActionIntended;
            if (k == "ACTION_EXECUTION") return EventKind::ActionExecution;
            if (k == "ACTION_EXECUTION_ACK") return EventKind::ActionExecutionAck;
            if (k == "EFFECT_OBSERVATION") return EventKind::EffectObservation;
            if (k == "EPISTEMIC_ACTION") return EventKind::EpistemicAction;
            if (k == "REINTERPRETATION") return EventKind::Reinterpretation;
            if (k == "ADAPTATION") return EventKind::Adaptation;
            if (k == "CONSTITUTIVE_WARNING") return EventKind::ConstitutiveWarning;
            if (k == "CONSTITUTIVE_REPAIR") return EventKind::ConstitutiveRepair;
            if (k == "COHERENCE_RESTORED") return EventKind::CoherenceRestored;
            return std::nullopt;
        };

        while (std::getline(in, line)) {
            if (line.empty()) continue;
            if (line.starts_with("#")) {
                continue; // Skip comments and format version header
            }

            std::stringstream ss(line);
            std::string id, kind_str, identity, time_str, prev_digest, p_digest, e_digest, auth_id, auth_epoch, causal_str, evidence_str, payload_esc;

            if (!std::getline(ss, id, '|') ||
                !std::getline(ss, kind_str, '|') ||
                !std::getline(ss, identity, '|') ||
                !std::getline(ss, time_str, '|') ||
                !std::getline(ss, prev_digest, '|') ||
                !std::getline(ss, p_digest, '|') ||
                !std::getline(ss, e_digest, '|') ||
                !std::getline(ss, auth_id, '|') ||
                !std::getline(ss, auth_epoch, '|') ||
                !std::getline(ss, causal_str, '|') ||
                !std::getline(ss, evidence_str, '|') ||
                !std::getline(ss, payload_esc)) {
                // Malformed / truncated line
                return std::unexpected(core::EnteError::HistoryCorrupt);
            }

            auto opt_kind = parse_kind(kind_str);
            if (!opt_kind.has_value()) {
                return std::unexpected(core::EnteError::HistoryCorrupt);
            }

            core::LogicalTime log_time = 0;
            auto [ptr, ec] = std::from_chars(time_str.data(), time_str.data() + time_str.size(), log_time);
            if (ec != std::errc()) {
                return std::unexpected(core::EnteError::HistoryCorrupt);
            }

            std::vector<core::EventId> causal_vec;
            if (!causal_str.empty()) {
                std::stringstream css(causal_str);
                std::string item;
                while (std::getline(css, item, ',')) {
                    if (!item.empty()) causal_vec.emplace_back(item);
                }
            }

            std::vector<core::EvidenceId> evidence_vec;
            if (!evidence_str.empty()) {
                std::stringstream ess(evidence_str);
                std::string item;
                while (std::getline(ess, item, ',')) {
                    if (!item.empty()) evidence_vec.emplace_back(item);
                }
            }

            std::string raw_payload = unescape_payload(payload_esc);

            HistoryEvent ev{
                .id = core::EventId(id),
                .kind = *opt_kind,
                .identity = core::IdentityId(identity),
                .logical_time = log_time,
                .previous_event_digest = core::Digest(prev_digest),
                .causal_predecessors = std::move(causal_vec),
                .evidence_refs = std::move(evidence_vec),
                .authority_id = auth_id,
                .authority_epoch = auth_epoch,
                .payload_content = std::move(raw_payload),
                .payload_digest = core::Digest(p_digest),
                .event_digest = core::Digest(e_digest)
            };

            auto app_res = rec.append(std::move(ev));
            if (!app_res.has_value()) {
                return std::unexpected(app_res.error());
            }
        }

        if (rec.empty()) {
            return std::unexpected(core::EnteError::HistoryGap);
        }

        if (!rec.verify_integrity()) {
            return std::unexpected(core::EnteError::HistoryCorrupt);
        }

        return rec;
    } catch (...) {
        return std::unexpected(core::EnteError::HistoryCorrupt);
    }
}

} // namespace ente::history
